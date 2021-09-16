﻿#include "TaskConfiger.h"

#include <algorithm>

#include <json.h>

#include "AsstDef.h"
#include "Logger.hpp"
#include "Configer.h"

using namespace asst;

bool TaskConfiger::set_param(const std::string& type, const std::string& param, const std::string& value)
{
	// 暂时只用到了这些，总的参数太多了，后面要用啥再加上
	if (type == "task.type") {
		if (m_all_tasks_info.find(param) == m_all_tasks_info.cend()) {
			return false;
		}
		auto task_info_ptr = m_all_tasks_info[param];
		std::string type = value;
		std::transform(type.begin(), type.end(), type.begin(), std::tolower);
		if (type == "clickself") {
			task_info_ptr->action = ProcessTaskAction::ClickSelf;
		}
		else if (type == "clickrand") {
			task_info_ptr->action = ProcessTaskAction::ClickRand;
		}
		else if (type == "donothing" || type.empty()) {
			task_info_ptr->action = ProcessTaskAction::DoNothing;
		}
		else if (type == "stop") {
			task_info_ptr->action = ProcessTaskAction::Stop;
		}
		else if (type == "clickrect") {
			task_info_ptr->action = ProcessTaskAction::ClickRect;
		}
		else {
			DebugTraceError("Task", param, "'s type error:", type);
			return false;
		}
	}
	else if (type == "task.maxTimes") {
		if (m_all_tasks_info.find(param) == m_all_tasks_info.cend()) {
			return false;
		}
		m_all_tasks_info[param]->max_times = std::stoi(value);
	}
	return true;
}

bool asst::TaskConfiger::parse(json::value&& json)
{
	json::value root = std::move(json);

	for (auto&& [name, task_json] : root.as_object()) {
		std::string algorithm_str = task_json.get("algorithm", "matchtemplate");
		std::transform(algorithm_str.begin(), algorithm_str.end(), algorithm_str.begin(), std::tolower);
		AlgorithmType algorithm = AlgorithmType::Invaild;
		if (algorithm_str == "matchtemplate") {
			algorithm = AlgorithmType::MatchTemplate;
		}
		else if (algorithm_str == "justreturn") {
			algorithm = AlgorithmType::JustReturn;
		}
		else if (algorithm_str == "ocrdetect") {
			algorithm = AlgorithmType::OcrDetect;
		}
		//else if (algorithm_str == "comparehist") {} // CompareHist是MatchTemplate的衍生算法，不应作为单独的配置参数出现
		else {
			DebugTraceError("Algorithm error:", algorithm_str);
			return false;
		}

		std::shared_ptr<TaskInfo> task_info_ptr = nullptr;
		switch (algorithm) {
		case AlgorithmType::JustReturn:
			task_info_ptr = std::make_shared<TaskInfo>();
			break;
		case AlgorithmType::MatchTemplate:
		{
			auto match_task_info_ptr = std::make_shared<MatchTaskInfo>();
			match_task_info_ptr->template_filename = task_json["template"].as_string();
			match_task_info_ptr->templ_threshold = task_json.get("templThreshold", Configer::TemplThresholdDefault);
			match_task_info_ptr->hist_threshold = task_json.get("histThreshold", Configer::HistThresholdDefault);
			match_task_info_ptr->cache = task_json.get("cache", true);
			task_info_ptr = match_task_info_ptr;
		}
		break;
		case AlgorithmType::OcrDetect:
		{
			auto ocr_task_info_ptr = std::make_shared<OcrTaskInfo>();
			for (const json::value& text : task_json["text"].as_array())
			{
				ocr_task_info_ptr->text.emplace_back(text.as_string());
			}
			ocr_task_info_ptr->need_match = task_json.get("need_match", false);
			if (task_json.exist("ocrReplace"))
			{
				for (const auto& [key, value] : task_json["ocrReplace"].as_object()) {
					ocr_task_info_ptr->replace_map.emplace(key, value.as_string());
				}
			}
			task_info_ptr = ocr_task_info_ptr;
		}
		break;
		}
		task_info_ptr->algorithm = algorithm;
		task_info_ptr->name = name;
		std::string action = task_json["action"].as_string();
		std::transform(action.begin(), action.end(), action.begin(), std::tolower);
		if (action == "clickself") {
			task_info_ptr->action = ProcessTaskAction::ClickSelf;
		}
		else if (action == "clickrand") {
			task_info_ptr->action = ProcessTaskAction::ClickRand;
		}
		else if (action == "donothing" || action.empty()) {
			task_info_ptr->action = ProcessTaskAction::DoNothing;
		}
		else if (action == "stop") {
			task_info_ptr->action = ProcessTaskAction::Stop;
		}
		else if (action == "clickrect") {
			task_info_ptr->action = ProcessTaskAction::ClickRect;
			json::value& area_json = task_json["specificArea"];
			task_info_ptr->specific_area = Rect(
				area_json[0].as_integer(),
				area_json[1].as_integer(),
				area_json[2].as_integer(),
				area_json[3].as_integer());
		}
		else if (action == "printwindow") {
			task_info_ptr->action = ProcessTaskAction::PrintWindow;
		}
		else if (action == "swipetotheleft") {
			task_info_ptr->action = ProcessTaskAction::SwipeToTheLeft;
		}
		else if (action == "swipetotheright") {
			task_info_ptr->action = ProcessTaskAction::SwipeToTheRight;
		}
		else {
			DebugTraceError("Task:", name, "error:", action);
			return false;
		}

		task_info_ptr->max_times = task_json.get("maxTimes", INT_MAX);
		if (task_json.exist("exceededNext")) {
			json::array& excceed_next_arr = task_json["exceededNext"].as_array();
			for (const json::value& excceed_next : excceed_next_arr) {
				task_info_ptr->exceeded_next.emplace_back(excceed_next.as_string());
			}
		}
		else {
			task_info_ptr->exceeded_next.emplace_back("Stop");
		}
		task_info_ptr->pre_delay = task_json.get("preDelay", 0);
		task_info_ptr->rear_delay = task_json.get("rearDelay", 0);
		if (task_json.exist("reduceOtherTimes")) {
			json::array& reduce_arr = task_json["reduceOtherTimes"].as_array();
			for (const json::value& reduce : reduce_arr) {
				task_info_ptr->reduce_other_times.emplace_back(reduce.as_string());
			}
		}
		if (task_json.exist("identifyArea")) {
			json::array& area_arr = task_json["identifyArea"].as_array();
			task_info_ptr->identify_area = Rect(
				area_arr[0].as_integer(),
				area_arr[1].as_integer(),
				area_arr[2].as_integer(),
				area_arr[3].as_integer());
		}

		json::array& next_arr = task_json["next"].as_array();
		for (const json::value& next : next_arr) {
			task_info_ptr->next.emplace_back(next.as_string());
		}

		m_all_tasks_info.emplace(name, task_info_ptr);
	}

}