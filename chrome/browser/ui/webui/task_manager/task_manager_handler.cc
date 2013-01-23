// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/task_manager/task_manager_handler.h"

#include <algorithm>
#include <functional>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/string_number_conversions.h"
#include "base/values.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/browser/ui/host_desktop.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_ui.h"
#include "ui/gfx/image/image_skia.h"
#include "webkit/glue/webpreferences.h"
#include "ui/webui/web_ui_util.h"

namespace {

struct ColumnType {
  const char* column_id;
  // Whether the column has the real value separately or not, instead of the
  // formatted value to display.
  const bool has_real_value;
  // Whether the column has single datum or multiple data in each group.
  const bool has_multiple_data;
};

const ColumnType kColumnsList[] = {
  {"type", false, false},
  {"processId", true, false},
  {"cpuUsage", true, false},
  {"physicalMemory", true, false},
  {"sharedMemory", true, false},
  {"privateMemory", true, false},
  {"webCoreImageCacheSize", true, false},
  {"webCoreImageCacheSize", true, false},
  {"webCoreScriptsCacheSize", true, false},
  {"webCoreCSSCacheSize", true, false},
  {"sqliteMemoryUsed", true, false},
  {"v8MemoryAllocatedSize", true, false},
  {"icon", false, true},
  {"title", false, true},
  {"profileName", false, true},
  {"networkUsage", true, true},
  {"fps", true, true},
  {"videoMemory", true, false},
  {"goatsTeleported", true, true},
  {"canInspect", false, true},
  {"canActivate", false, true}
};

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
  OnGroupChanged(0, model_->GroupCount());
}

void TaskManagerHandler::OnItemsChanged(const int start, const int length) {
  OnGroupChanged(0, model_->GroupCount());
}

void TaskManagerHandler::OnItemsAdded(const int start, const int length) {
}

void TaskManagerHandler::OnItemsRemoved(const int start, const int length) {
}

void TaskManagerHandler::RegisterMessages() {
  web_ui()->RegisterMessageCallback("killProcesses",
      base::Bind(&TaskManagerHandler::HandleKillProcesses,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("inspect",
      base::Bind(&TaskManagerHandler::HandleInspect,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("activatePage",
      base::Bind(&TaskManagerHandler::HandleActivatePage,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("openAboutMemory",
      base::Bind(&TaskManagerHandler::OpenAboutMemory,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("enableTaskManager",
      base::Bind(&TaskManagerHandler::EnableTaskManager,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("disableTaskManager",
      base::Bind(&TaskManagerHandler::DisableTaskManager,
                 base::Unretained(this)));
  web_ui()->RegisterMessageCallback("setUpdateColumn",
      base::Bind(&TaskManagerHandler::HandleSetUpdateColumn,
                 base::Unretained(this)));
}

static int parseIndex(const Value* value) {
  int index = -1;
  string16 string16_index;
  double double_index;
  if (value->GetAsString(&string16_index)) {
    bool converted = base::StringToInt(string16_index, &index);
    DCHECK(converted);
  } else if (value->GetAsDouble(&double_index)) {
    index = static_cast<int>(double_index);
  } else {
    value->GetAsInteger(&index);
  }
  return index;
}

void TaskManagerHandler::HandleKillProcesses(const ListValue* unique_ids) {
  for (ListValue::const_iterator i = unique_ids->begin();
       i != unique_ids->end(); ++i) {
    int unique_id = parseIndex(*i);
    int resource_index = model_->GetResourceIndexByUniqueId(unique_id);
    if (resource_index == -1)
      continue;

    task_manager_->KillProcess(resource_index);
  }
}

void TaskManagerHandler::HandleActivatePage(const ListValue* unique_ids) {
  for (ListValue::const_iterator i = unique_ids->begin();
       i != unique_ids->end(); ++i) {
    int unique_id = parseIndex(*i);
    int resource_index = model_->GetResourceIndexByUniqueId(unique_id);
    if (resource_index == -1)
      continue;

    task_manager_->ActivateProcess(resource_index);
    break;
  }
}

void TaskManagerHandler::HandleInspect(const ListValue* unique_ids) {
  for (ListValue::const_iterator i = unique_ids->begin();
       i != unique_ids->end(); ++i) {
    int unique_id = parseIndex(*i);
    int resource_index = model_->GetResourceIndexByUniqueId(unique_id);
    if (resource_index == -1)
      continue;

    if (model_->CanInspect(resource_index))
      model_->Inspect(resource_index);
    break;
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

  OnGroupChanged(0, model_->GroupCount());

  model_->AddObserver(this);
  model_->StartUpdating();

  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_TASK_MANAGER_WINDOW_READY,
      content::Source<TaskManagerModel>(model_),
      content::NotificationService::NoDetails());
}

void TaskManagerHandler::OpenAboutMemory(const ListValue* indexes) {
  content::RenderViewHost* rvh =
      web_ui()->GetWebContents()->GetRenderViewHost();
  if (rvh) {
    webkit_glue::WebPreferences webkit_prefs = rvh->GetWebkitPreferences();
    webkit_prefs.allow_scripts_to_close_windows = true;
    rvh->UpdateWebkitPreferences(webkit_prefs);
  } else {
    DCHECK(false);
  }

  task_manager_->OpenAboutMemory(chrome::GetActiveDesktop());
}

void TaskManagerHandler::HandleSetUpdateColumn(const ListValue* args) {
  DCHECK_EQ(2U, args->GetSize());

  bool ret = true;
  std::string column_id;
  ret &= args->GetString(0, &column_id);
  bool is_enabled;
  ret &= args->GetBoolean(1, &is_enabled);
  DCHECK(ret);

  if (is_enabled)
    enabled_columns_.insert(column_id);
  else
    enabled_columns_.erase(column_id);
}

// TaskManagerHandler, private: -----------------------------------------------

bool TaskManagerHandler::is_alive() {
  return web_ui()->GetWebContents()->GetRenderViewHost() != NULL;
}

void TaskManagerHandler::OnGroupChanged(const int group_start,
                                        const int group_length) {
  base::FundamentalValue start_value(group_start);
  base::FundamentalValue length_value(group_length);
  base::ListValue tasks_value;

  for (int i = 0; i < group_length; ++i) {
    tasks_value.Append(CreateTaskGroupValue(group_start + i));
  }

  if (is_enabled_ && is_alive()) {
    web_ui()->CallJavascriptFunction("taskChanged",
                                     start_value, length_value, tasks_value);
  }
}

void TaskManagerHandler::OnGroupAdded(const int group_start,
                                      const int group_length) {
}

void TaskManagerHandler::OnGroupRemoved(const int group_start,
                                        const int group_length) {
}

void TaskManagerHandler::OnReadyPeriodicalUpdate() {
}

base::DictionaryValue* TaskManagerHandler::CreateTaskGroupValue(
    int group_index) {
  DictionaryValue* val = new DictionaryValue();

  if (group_index >= model_->GroupCount())
     return val;

  int index = model_->GetResourceIndexForGroup(group_index, 0);
  int length = model_->GetGroupRangeForResource(index).second;

  // Forces to set following 3 columns regardless of |enable_columns|.
  val->SetInteger("index", index);
  val->SetBoolean("isBackgroundResource",
                  model_->IsBackgroundResource(index));
  CreateGroupColumnList("processId", index, 1, val);
  CreateGroupColumnList("type", index, length, val);
  CreateGroupColumnList("uniqueId", index, length, val);

  for (size_t i = 0; i < arraysize(kColumnsList); ++i) {
    const std::string column_id = kColumnsList[i].column_id;

    if (enabled_columns_.find(column_id) == enabled_columns_.end())
      continue;

    int column_length = kColumnsList[i].has_multiple_data ? length : 1;
    CreateGroupColumnList(column_id, index, column_length, val);

    if (kColumnsList[i].has_real_value)
      CreateGroupColumnList(column_id + "Value", index, column_length, val);
  }

  return val;
}

void TaskManagerHandler::CreateGroupColumnList(const std::string& column_name,
                                               const int index,
                                               const int length,
                                               DictionaryValue* val) {
  ListValue* list = new ListValue();
  for (int i = index; i < (index + length); ++i) {
    list->Append(CreateColumnValue(column_name, i));
  }
  val->Set(column_name, list);
}

base::Value* TaskManagerHandler::CreateColumnValue(
    const std::string& column_name,
    const int i) {
  if (column_name == "uniqueId")
    return Value::CreateIntegerValue(model_->GetResourceUniqueId(i));
  if (column_name == "type") {
    return Value::CreateStringValue(
        TaskManager::Resource::GetResourceTypeAsString(
        model_->GetResourceType(i)));
  }
  if (column_name == "processId")
    return Value::CreateStringValue(model_->GetResourceProcessId(i));
  if (column_name == "processIdValue")
    return Value::CreateIntegerValue(model_->GetProcessId(i));
  if (column_name == "cpuUsage")
    return Value::CreateStringValue(model_->GetResourceCPUUsage(i));
  if (column_name == "cpuUsageValue")
    return Value::CreateDoubleValue(model_->GetCPUUsage(i));
  if (column_name == "privateMemory")
    return Value::CreateStringValue(model_->GetResourcePrivateMemory(i));
  if (column_name == "privateMemoryValue") {
    size_t private_memory;
    model_->GetPrivateMemory(i, &private_memory);
    return Value::CreateDoubleValue(private_memory);
  }
  if (column_name == "sharedMemory")
    return Value::CreateStringValue(model_->GetResourceSharedMemory(i));
  if (column_name == "sharedMemoryValue") {
    size_t shared_memory;
    model_->GetSharedMemory(i, &shared_memory);
    return Value::CreateDoubleValue(shared_memory);
  }
  if (column_name == "physicalMemory")
    return Value::CreateStringValue(model_->GetResourcePhysicalMemory(i));
  if (column_name == "physicalMemoryValue") {
    size_t physical_memory;
    model_->GetPhysicalMemory(i, &physical_memory);
    return Value::CreateDoubleValue(physical_memory);
  }
  if (column_name == "icon") {
    ui::ScaleFactor icon_scale_factor = web_ui()->GetDeviceScaleFactor();
    const gfx::ImageSkia& image = model_->GetResourceIcon(i);
    const gfx::ImageSkiaRep image_rep =
        image.GetRepresentation(icon_scale_factor);
    return Value::CreateStringValue(
        webui::GetBitmapDataUrl(image_rep.sk_bitmap()));
  }
  if (column_name == "title")
    return Value::CreateStringValue(model_->GetResourceTitle(i));
  if (column_name == "profileName")
    return Value::CreateStringValue(model_->GetResourceProfileName(i));
  if (column_name == "networkUsage")
    return Value::CreateStringValue(model_->GetResourceNetworkUsage(i));
  if (column_name == "networkUsageValue")
    return Value::CreateDoubleValue(model_->GetNetworkUsage(i));
  if (column_name == "webCoreImageCacheSize") {
    return Value::CreateStringValue(
        model_->GetResourceWebCoreImageCacheSize(i));
  }
  if (column_name == "webCoreImageCacheSizeValue") {
    WebKit::WebCache::ResourceTypeStats resource_stats;
    model_->GetWebCoreCacheStats(i, &resource_stats);
    return Value::CreateDoubleValue(resource_stats.images.size);
  }
  if (column_name == "webCoreScriptsCacheSize") {
    return Value::CreateStringValue(
        model_->GetResourceWebCoreScriptsCacheSize(i));
  }
  if (column_name == "webCoreScriptsCacheSizeValue") {
    WebKit::WebCache::ResourceTypeStats resource_stats;
    model_->GetWebCoreCacheStats(i, &resource_stats);
    return Value::CreateDoubleValue(resource_stats.scripts.size);
  }
  if (column_name == "webCoreCSSCacheSize")
    return Value::CreateStringValue(model_->GetResourceWebCoreCSSCacheSize(i));
  if (column_name == "webCoreCSSCacheSizeValue") {
    WebKit::WebCache::ResourceTypeStats resource_stats;
    model_->GetWebCoreCacheStats(i, &resource_stats);
    return Value::CreateDoubleValue(resource_stats.cssStyleSheets.size);
  }
  if (column_name == "fps")
    return Value::CreateStringValue(model_->GetResourceFPS(i));
  if (column_name == "fpsValue") {
    float fps;
    model_->GetFPS(i, &fps);
    return Value::CreateDoubleValue(fps);
  }
  if (column_name == "videoMemory")
    return Value::CreateStringValue(model_->GetResourceVideoMemory(i));
  if (column_name == "videoMemoryValue") {
    size_t video_memory;
    bool has_duplicates;
    double value;
    if (model_->GetVideoMemory(i, &video_memory, &has_duplicates))
      value = static_cast<double>(video_memory);
    else
      value = 0;
    return Value::CreateDoubleValue(value);
  }
  if (column_name == "sqliteMemoryUsed")
    return Value::CreateStringValue(model_->GetResourceSqliteMemoryUsed(i));
  if (column_name == "sqliteMemoryUsedValue") {
    size_t sqlite_memory;
    model_->GetSqliteMemoryUsedBytes(i, &sqlite_memory);
    return Value::CreateDoubleValue(sqlite_memory);
  }
  if (column_name == "goatsTeleported")
    return Value::CreateStringValue(model_->GetResourceGoatsTeleported(i));
  if (column_name == "goatsTeleportedValue")
    return Value::CreateIntegerValue(model_->GetGoatsTeleported(i));
  if (column_name == "v8MemoryAllocatedSize") {
    return Value::CreateStringValue(
        model_->GetResourceV8MemoryAllocatedSize(i));
  }
  if (column_name == "v8MemoryAllocatedSizeValue") {
    size_t v8_memory;
    model_->GetV8Memory(i, &v8_memory);
    return Value::CreateDoubleValue(v8_memory);
  }
  if (column_name == "canInspect")
    return Value::CreateBooleanValue(model_->CanInspect(i));
  if (column_name == "canActivate")
    return Value::CreateBooleanValue(model_->CanActivate(i));

  NOTREACHED();
  return NULL;
}
