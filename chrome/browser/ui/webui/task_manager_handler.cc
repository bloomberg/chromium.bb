// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/task_manager_handler.h"

#include <algorithm>
#include <functional>
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/browser/ui/webui/web_ui_util.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace {

static DictionaryValue* CreateTaskItemValue(const TaskManagerModel* tm,
                                            const int index) {
  size_t result;

  DictionaryValue* val = new DictionaryValue();

  val->SetInteger("index", index);
  val->SetString("processId", tm->GetResourceProcessId(index));
  val->SetString("title", tm->GetResourceTitle(index));
  val->SetString("cpuUsage", tm->GetResourceCPUUsage(index));
  val->SetString("networkUsage", tm->GetResourceNetworkUsage(index));
  val->SetDouble("networkUsageValue",
                 static_cast<double>(tm->GetNetworkUsage(index)));
  val->SetString("privateMemory", tm->GetResourcePrivateMemory(index));
  tm->GetPrivateMemory(index, &result);
  val->SetDouble("privateMemoryValue", static_cast<double>(result));
  val->SetString("icon",
                 web_ui_util::GetImageDataUrl(tm->GetResourceIcon(index)));
  val->SetBoolean("is_resource_first_in_group",
                  tm->IsResourceFirstInGroup(index));

  return val;
}

}  // namespace

TaskManagerHandler::TaskManagerHandler(TaskManager* tm)
    : task_manager_(tm),
      model_(tm->model()),
      is_enabled_(false) {
}

TaskManagerHandler::~TaskManagerHandler() {
  DisableTaskManager(NULL);
}

// DownloadsDOMHandler, public: -----------------------------------------------

void TaskManagerHandler::OnModelChanged() {
  const int count = model_->ResourceCount();

  for (int i = 0; i < count; i++) {
    ListValue results_value;
    results_value.Append(CreateTaskItemValue(model_, i));
    web_ui_->CallJavascriptFunction("taskChanged", results_value);
  }
}

void TaskManagerHandler::OnItemsChanged(int start, int length) {
  FundamentalValue start_value(start);
  FundamentalValue length_value(length);
  ListValue tasks_value;
  for (int i = start; i < start + length; i++) {
    tasks_value.Append(CreateTaskItemValue(model_, i));
  }
  if (is_enabled_) {
    web_ui_->CallJavascriptFunction("taskChanged",
                                    start_value, length_value, tasks_value);
  }
}

void TaskManagerHandler::OnItemsAdded(int start, int length) {
  FundamentalValue start_value(start);
  FundamentalValue length_value(length);
  ListValue tasks_value;
  for (int i = start; i < start + length; i++) {
    tasks_value.Append(CreateTaskItemValue(model_, i));
  }
  if (is_enabled_) {
    web_ui_->CallJavascriptFunction("taskAdded",
                                    start_value, length_value, tasks_value);
  }
}

void TaskManagerHandler::OnItemsRemoved(int start, int length) {
  FundamentalValue start_value(start);
  FundamentalValue length_value(length);
  if (is_enabled_)
    web_ui_->CallJavascriptFunction("taskRemoved", start_value, length_value);
}

void TaskManagerHandler::Init() {
}

void TaskManagerHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("killProcess",
      NewCallback(this, &TaskManagerHandler::HandleKillProcess));
  web_ui_->RegisterMessageCallback("openAboutMemory",
      NewCallback(this, &TaskManagerHandler::OpenAboutMemory));
  web_ui_->RegisterMessageCallback("enableTaskManager",
      NewCallback(this, &TaskManagerHandler::EnableTaskManager));
  web_ui_->RegisterMessageCallback("disableTaskManager",
      NewCallback(this, &TaskManagerHandler::DisableTaskManager));
}

void TaskManagerHandler::HandleKillProcess(const ListValue* indexes) {
  for (ListValue::const_iterator i = indexes->begin();
       i != indexes->end(); i++) {
    int index = -1;
    string16 string16_index;
    double double_index;
    if ((*i)->GetAsString(&string16_index)) {
      base::StringToInt(string16_index, &index);
    } else if ((*i)->GetAsDouble(&double_index)) {
      index = static_cast<int>(double_index);
    } else {
      (*i)->GetAsInteger(&index);
    }
    LOG(INFO) << "kill PID:" << index;
    if (index != -1) {
      task_manager_->KillProcess(index);
    }
  }
}

void TaskManagerHandler::DisableTaskManager(const ListValue* indexes) {
  if (!is_enabled_)
    return;

  is_enabled_ = false;
  model_->StopUpdating();
  model_->RemoveObserver(this);
}

void TaskManagerHandler::EnableTaskManager(const ListValue* indexes) {
  if (is_enabled_)
    return;

  is_enabled_ = true;
  model_->AddObserver(this);
  model_->StartUpdating();
}

void TaskManagerHandler::OpenAboutMemory(const ListValue* indexes) {
  task_manager_->OpenAboutMemory();
}

