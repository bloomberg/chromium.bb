// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/task_manager_handler.h"

#include <algorithm>
#include <functional>
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/browser/ui/webui/web_ui_util.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/notification_service.h"
#include "content/common/notification_source.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "webkit/glue/webpreferences.h"

namespace {

static Value* CreateColumnValue(const TaskManagerModel* tm,
                                const std::string column_name,
                                const int i) {
  if (column_name == "processId")
    return Value::CreateStringValue(tm->GetResourceProcessId(i));
  if (column_name == "processIdValue")
    return Value::CreateIntegerValue(tm->GetProcessId(i));
  if (column_name == "cpuUsage")
    return Value::CreateStringValue(tm->GetResourceCPUUsage(i));
  if (column_name == "cpuUsageValue")
    return Value::CreateDoubleValue(tm->GetCPUUsage(i));
  if (column_name == "privateMemory")
    return Value::CreateStringValue(tm->GetResourcePrivateMemory(i));
  if (column_name == "privateMemoryValue") {
    size_t private_memory;
    tm->GetPrivateMemory(i, &private_memory);
    return Value::CreateDoubleValue(private_memory);
  }
  if (column_name == "sharedMemory")
    return Value::CreateStringValue(tm->GetResourceSharedMemory(i));
  if (column_name == "sharedMemoryValue") {
    size_t shared_memory;
    tm->GetSharedMemory(i, &shared_memory);
    return Value::CreateDoubleValue(shared_memory);
  }
  if (column_name == "physicalMemory")
    return Value::CreateStringValue(tm->GetResourcePhysicalMemory(i));
  if (column_name == "physicalMemoryValue") {
    size_t physical_memory;
    tm->GetPhysicalMemory(i, &physical_memory);
    return Value::CreateDoubleValue(physical_memory);
  }
  if (column_name == "icon")
    return Value::CreateStringValue(
               web_ui_util::GetImageDataUrl(tm->GetResourceIcon(i)));
  if (column_name == "title")
    return Value::CreateStringValue(tm->GetResourceTitle(i));
  if (column_name == "profileName")
    return Value::CreateStringValue(tm->GetResourceProfileName(i));
  if (column_name == "networkUsage")
    return Value::CreateStringValue(tm->GetResourceNetworkUsage(i));
  if (column_name == "networkUsageValue")
    return Value::CreateDoubleValue(tm->GetNetworkUsage(i));
  if (column_name == "webCoreImageCacheSize")
    return Value::CreateStringValue(tm->GetResourceWebCoreImageCacheSize(i));
  if (column_name == "webCoreImageCacheSizeValue") {
    WebKit::WebCache::ResourceTypeStats resource_stats;
    tm->GetWebCoreCacheStats(i, &resource_stats);
    return Value::CreateDoubleValue(resource_stats.images.size);
  }
  if (column_name == "webCoreScriptsCacheSize")
    return Value::CreateStringValue(tm->GetResourceWebCoreScriptsCacheSize(i));
  if (column_name == "webCoreScriptsCacheSizeValue") {
    WebKit::WebCache::ResourceTypeStats resource_stats;
    tm->GetWebCoreCacheStats(i, &resource_stats);
    return Value::CreateDoubleValue(resource_stats.scripts.size);
  }
  if (column_name == "webCoreCSSCacheSize")
    return Value::CreateStringValue(tm->GetResourceWebCoreCSSCacheSize(i));
  if (column_name == "webCoreCSSCacheSizeValue") {
    WebKit::WebCache::ResourceTypeStats resource_stats;
    tm->GetWebCoreCacheStats(i, &resource_stats);
    return Value::CreateDoubleValue(resource_stats.cssStyleSheets.size);
  }
  if (column_name == "fps")
    return Value::CreateStringValue(tm->GetResourceFPS(i));
  if (column_name == "fpsValue") {
    float fps;
    tm->GetFPS(i, &fps);
    return Value::CreateDoubleValue(fps);
  }
  if (column_name == "sqliteMemoryUsed")
    return Value::CreateStringValue(tm->GetResourceSqliteMemoryUsed(i));
  if (column_name == "sqliteMemoryUsedValue") {
    size_t sqlite_memory;
    tm->GetSqliteMemoryUsedBytes(i, &sqlite_memory);
    return Value::CreateDoubleValue(sqlite_memory);
  }
  if (column_name == "goatsTeleported")
    return Value::CreateStringValue(tm->GetResourceGoatsTeleported(i));
  if (column_name == "goatsTeleportedValue")
    return Value::CreateIntegerValue(tm->GetGoatsTeleported(i));
  if (column_name == "v8MemoryAllocatedSize")
    return Value::CreateStringValue(tm->GetResourceV8MemoryAllocatedSize(i));
  if (column_name == "v8MemoryAllocatedSizeValue") {
    size_t v8_memory;
    tm->GetV8Memory(i, &v8_memory);
    return Value::CreateDoubleValue(v8_memory);
  }

  NOTREACHED();
  return NULL;
}

static void CreateGroupColumnList(const TaskManagerModel* tm,
                                  const std::string column_name,
                                  const int index,
                                  const int length,
                                  DictionaryValue* val) {
  ListValue *list = new ListValue();
  for (int i = index; i < (index + length); i++) {
    list->Append(CreateColumnValue(tm, column_name, i));
  }
  val->Set(column_name, list);
}

static DictionaryValue* CreateTaskGroupValue(const TaskManagerModel* tm,
                                            const int group_index) {
  DictionaryValue* val = new DictionaryValue();

  const int group_count = tm->GroupCount();
  if (group_index >= group_count)
     return val;

  int index = tm->GetResourceIndexForGroup(group_index, 0);
  std::pair<int, int> group_range;
  group_range = tm->GetGroupRangeForResource(index);
  int length = group_range.second;

  val->SetInteger("index", index);
  val->SetInteger("group_range_start", group_range.first);
  val->SetInteger("group_range_length", group_range.second);
  val->SetInteger("index_in_group", index - group_range.first);
  val->SetBoolean("is_resource_first_in_group",
                  tm->IsResourceFirstInGroup(index));
  val->SetBoolean("isBackgroundResource",
                  tm->IsBackgroundResource(index));

  // Columns which have one datum in each group.
  CreateGroupColumnList(tm, "processId", index, 1, val);
  CreateGroupColumnList(tm, "processIdValue", index, 1, val);
  CreateGroupColumnList(tm, "cpuUsage", index, 1, val);
  CreateGroupColumnList(tm, "cpuUsageValue", index, 1, val);
  CreateGroupColumnList(tm, "physicalMemory", index, 1, val);
  CreateGroupColumnList(tm, "physicalMemoryValue", index, 1, val);
  CreateGroupColumnList(tm, "sharedMemory", index, 1, val);
  CreateGroupColumnList(tm, "sharedMemoryValue", index, 1, val);
  CreateGroupColumnList(tm, "privateMemory", index, 1, val);
  CreateGroupColumnList(tm, "privateMemoryValue", index, 1, val);
  CreateGroupColumnList(tm, "webCoreImageCacheSize", index, 1, val);
  CreateGroupColumnList(tm, "webCoreImageCacheSizeValue", index, 1, val);
  CreateGroupColumnList(tm, "webCoreScriptsCacheSize", index, 1, val);
  CreateGroupColumnList(tm, "webCoreScriptsCacheSizeValue", index, 1, val);
  CreateGroupColumnList(tm, "webCoreCSSCacheSize", index, 1, val);
  CreateGroupColumnList(tm, "webCoreCSSCacheSizeValue", index, 1, val);
  CreateGroupColumnList(tm, "sqliteMemoryUsed", index, 1, val);
  CreateGroupColumnList(tm, "sqliteMemoryUsedValue", index, 1, val);
  CreateGroupColumnList(tm, "v8MemoryAllocatedSize", index, 1, val);
  CreateGroupColumnList(tm, "v8MemoryAllocatedSizeValue", index, 1, val);

  // Columns which have some data in each group.
  CreateGroupColumnList(tm, "icon", index, length, val);
  CreateGroupColumnList(tm, "title", index, length, val);
  CreateGroupColumnList(tm, "profileName", index, length, val);
  CreateGroupColumnList(tm, "networkUsage", index, length, val);
  CreateGroupColumnList(tm, "networkUsageValue", index, length, val);
  CreateGroupColumnList(tm, "fps", index, length, val);
  CreateGroupColumnList(tm, "fpsValue", index, length, val);
  CreateGroupColumnList(tm, "goatsTeleported", index, length, val);
  CreateGroupColumnList(tm, "goatsTeleportedValue", index, length, val);

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

// TaskManagerHandler, public: -----------------------------------------------

void TaskManagerHandler::OnModelChanged() {
  const int count = model_->GroupCount();

  base::FundamentalValue start_value(0);
  base::FundamentalValue length_value(count);
  base::ListValue tasks_value;
  for (int i = 0; i < count; ++i)
    tasks_value.Append(CreateTaskGroupValue(model_, i));

  if (is_enabled_) {
    web_ui_->CallJavascriptFunction("taskChanged",
                                    start_value, length_value, tasks_value);
  }
}

void TaskManagerHandler::OnItemsChanged(const int start, const int length) {
  UpdateResourceGroupTable(start, length);

  // Converts from an index of resources to an index of groups.
  int group_start = model_->GetGroupIndexForResource(start);
  int group_end = model_->GetGroupIndexForResource(start + length - 1);

  OnGroupChanged(group_start, group_end - group_start + 1);
}

void TaskManagerHandler::OnItemsAdded(const int start, const int length) {
  UpdateResourceGroupTable(start, length);

  // Converts from an index of resources to an index of groups.
  int group_start = model_->GetGroupIndexForResource(start);
  int group_end = model_->GetGroupIndexForResource(start + length - 1);

  // First group to add does not contain all the items in the group. Because the
  // first item to add and the previous one are in same group.
  if (!model_->IsResourceFirstInGroup(start)) {
    OnGroupChanged(group_start, 1);
    if (group_start == group_end)
      return;
    else
      group_start++;
  }

  // Last group to add does not contain all the items in the group. Because the
  // last item to add and the next one are in same group.
  if (!model_->IsResourceLastInGroup(start + length - 1)) {
    OnGroupChanged(group_end, 1);
    if (group_start == group_end)
      return;
    else
      group_end--;
  }

  OnGroupAdded(group_start, group_end - group_start + 1);
}

void TaskManagerHandler::OnItemsRemoved(const int start, const int length) {
  // Returns if this is called before updating |resource_to_group_table_|.
  if (resource_to_group_table_.size() < static_cast<size_t>(start + length))
    return;

  // Converts from an index of resources to an index of groups.
  int group_start = resource_to_group_table_[start];
  int group_end = resource_to_group_table_[start + length - 1];

  // First group to remove does not contain all the items in the group. Because
  // the first item to remove and the previous one are in same group.
  if (start != 0 && group_start == resource_to_group_table_[start - 1]) {
    OnGroupChanged(group_start, 1);
    if (group_start == group_end)
      return;
    else
      group_start++;
  }

  // Last group to remove does not contain all the items in the group. Because
  // the last item to remove and the next one are in same group.
  if (start + length != model_->ResourceCount() &&
      group_end == resource_to_group_table_[start + length]) {
    OnGroupChanged(group_end, 1);
    if (group_start == group_end)
      return;
    else
      group_end--;
  }

  std::vector<int>::iterator it_first =
      resource_to_group_table_.begin() + start;
  std::vector<int>::iterator it_last = it_first + length - 1;
  resource_to_group_table_.erase(it_first, it_last);

  OnGroupRemoved(group_start, group_end - group_start + 1);
}

void TaskManagerHandler::Init() {
}

void TaskManagerHandler::RegisterMessages() {
  web_ui_->RegisterMessageCallback("killProcess",
      base::Bind(&TaskManagerHandler::HandleKillProcess,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("openAboutMemory",
      base::Bind(&TaskManagerHandler::OpenAboutMemory,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("enableTaskManager",
      base::Bind(&TaskManagerHandler::EnableTaskManager,
                 base::Unretained(this)));
  web_ui_->RegisterMessageCallback("disableTaskManager",
      base::Bind(&TaskManagerHandler::DisableTaskManager,
                 base::Unretained(this)));
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
    if (index == -1)
      continue;

    int resource_index = model_->GetResourceIndexForGroup(index, 0);
    if (resource_index == -1)
      continue;

    LOG(INFO) << "kill PID:" << model_->GetResourceProcessId(resource_index);
    task_manager_->KillProcess(resource_index);
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

  NotificationService::current()->Notify(
      chrome::NOTIFICATION_TASK_MANAGER_WINDOW_READY,
      Source<TaskManagerModel>(model_),
      NotificationService::NoDetails());
}

void TaskManagerHandler::OpenAboutMemory(const ListValue* indexes) {
  RenderViewHost* rvh = web_ui_->tab_contents()->render_view_host();
  if (rvh && rvh->delegate()) {
    WebPreferences webkit_prefs = rvh->delegate()->GetWebkitPrefs();
    webkit_prefs.allow_scripts_to_close_windows = true;
    rvh->UpdateWebkitPreferences(webkit_prefs);
  } else {
    DCHECK(false);
  }

  task_manager_->OpenAboutMemory();
}

// TaskManagerHandler, private: -----------------------------------------------

void TaskManagerHandler::UpdateResourceGroupTable(int start, int length) {
  if (resource_to_group_table_.size() < static_cast<size_t>(start)) {
    length += start - resource_to_group_table_.size();
    start = resource_to_group_table_.size();
  }

  // Makes room to fill group_index at first since inserting vector is too slow.
  std::vector<int>::iterator it = resource_to_group_table_.begin() + start;
  resource_to_group_table_.insert(it, static_cast<size_t>(length), -1);

  for (int i = start; i < start + length; i++) {
    const int group_index = model_->GetGroupIndexForResource(i);
    resource_to_group_table_[i] = group_index;
  }
}

void TaskManagerHandler::OnGroupChanged(const int group_start,
                                        const int group_length) {
  base::FundamentalValue start_value(group_start);
  base::FundamentalValue length_value(group_length);
  base::ListValue tasks_value;

  for (int i = 0; i < group_length; ++i)
    tasks_value.Append(CreateTaskGroupValue(model_, group_start + i));

  if (is_enabled_) {
    web_ui_->CallJavascriptFunction("taskChanged",
                                    start_value, length_value, tasks_value);
  }
}

void TaskManagerHandler::OnGroupAdded(const int group_start,
                                      const int group_length) {
  base::FundamentalValue start_value(group_start);
  base::FundamentalValue length_value(group_length);
  base::ListValue tasks_value;
  for (int i = 0; i < group_length; ++i)
    tasks_value.Append(CreateTaskGroupValue(model_, group_start + i));

  if (is_enabled_) {
    web_ui_->CallJavascriptFunction("taskAdded",
                                    start_value, length_value, tasks_value);
  }
}

void TaskManagerHandler::OnGroupRemoved(const int group_start,
                                        const int group_length) {
  base::FundamentalValue start_value(group_start);
  base::FundamentalValue length_value(group_length);
  if (is_enabled_)
    web_ui_->CallJavascriptFunction("taskRemoved", start_value, length_value);
}
