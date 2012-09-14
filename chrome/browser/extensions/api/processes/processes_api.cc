// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/processes/processes_api.h"

#include "base/callback.h"
#include "base/json/json_writer.h"
#include "base/message_loop.h"
#include "base/metrics/histogram.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"

#include "chrome/browser/extensions/api/processes/processes_api_constants.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/extensions/extension_function_util.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/extensions/extension_error_utils.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/result_codes.h"

namespace extensions {

namespace keys = processes_api_constants;
namespace errors = processes_api_constants;

namespace {

#if defined(ENABLE_TASK_MANAGER)

DictionaryValue* CreateCacheData(
    const WebKit::WebCache::ResourceTypeStat& stat) {

  DictionaryValue* cache = new DictionaryValue();
  cache->SetDouble(keys::kCacheSize, static_cast<double>(stat.size));
  cache->SetDouble(keys::kCacheLiveSize, static_cast<double>(stat.liveSize));
  return cache;
}

void SetProcessType(DictionaryValue* result,
                    TaskManagerModel* model,
                    int index) {
  // Determine process type.
  std::string type = keys::kProcessTypeOther;
  TaskManager::Resource::Type resource_type = model->GetResourceType(index);
  switch (resource_type) {
    case TaskManager::Resource::BROWSER:
      type = keys::kProcessTypeBrowser;
      break;
    case TaskManager::Resource::RENDERER:
      type = keys::kProcessTypeRenderer;
      break;
    case TaskManager::Resource::EXTENSION:
      type = keys::kProcessTypeExtension;
      break;
    case TaskManager::Resource::NOTIFICATION:
      type = keys::kProcessTypeNotification;
      break;
    case TaskManager::Resource::PLUGIN:
      type = keys::kProcessTypePlugin;
      break;
    case TaskManager::Resource::WORKER:
      type = keys::kProcessTypeWorker;
      break;
    case TaskManager::Resource::NACL:
      type = keys::kProcessTypeNacl;
      break;
    case TaskManager::Resource::UTILITY:
      type = keys::kProcessTypeUtility;
      break;
    case TaskManager::Resource::GPU:
      type = keys::kProcessTypeGPU;
      break;
    case TaskManager::Resource::PROFILE_IMPORT:
    case TaskManager::Resource::ZYGOTE:
    case TaskManager::Resource::SANDBOX_HELPER:
    case TaskManager::Resource::UNKNOWN:
      type = keys::kProcessTypeOther;
      break;
    default:
      NOTREACHED() << "Unknown resource type.";
  }
  result->SetString(keys::kTypeKey, type);
}

ListValue* GetTabsForProcess(int process_id) {
  ListValue* tabs_list = new ListValue();

  // The tabs list only makes sense for render processes, so if we don't find
  // one, just return the empty list.
  content::RenderProcessHost* rph =
      content::RenderProcessHost::FromID(process_id);
  if (rph == NULL)
    return tabs_list;

  int tab_id = -1;
  // We need to loop through all the RVHs to ensure we collect the set of all
  // tabs using this renderer process.
  content::RenderProcessHost::RenderWidgetHostsIterator iter(
      rph->GetRenderWidgetHostsIterator());
  for (; !iter.IsAtEnd(); iter.Advance()) {
    const content::RenderWidgetHost* widget = iter.GetCurrentValue();
    DCHECK(widget);
    if (!widget || !widget->IsRenderView())
      continue;

    content::RenderViewHost* host = content::RenderViewHost::From(
        const_cast<content::RenderWidgetHost*>(widget));
    content::WebContents* contents =
        content::WebContents::FromRenderViewHost(host);
    if (contents) {
      tab_id = ExtensionTabUtil::GetTabId(contents);
      if (tab_id != -1)
        tabs_list->Append(Value::CreateIntegerValue(tab_id));
    }
  }

  return tabs_list;
}

// This function creates a Process object to be returned to the extensions
// using these APIs. For memory details, which are not added by this function,
// the callers need to use AddMemoryDetails.
DictionaryValue* CreateProcessFromModel(int process_id,
                                        TaskManagerModel* model,
                                        int index,
                                        bool include_optional) {
  DictionaryValue* result = new DictionaryValue();
  size_t mem;

  result->SetInteger(keys::kIdKey, process_id);
  result->SetInteger(keys::kOsProcessIdKey, model->GetProcessId(index));
  SetProcessType(result, model, index);
  result->SetString(keys::kProfileKey,
      model->GetResourceProfileName(index));

  result->Set(keys::kTabsListKey, GetTabsForProcess(process_id));

  // If we don't need to include the optional properties, just return now.
  if (!include_optional)
    return result;

  result->SetDouble(keys::kCpuKey, model->GetCPUUsage(index));

  if (model->GetV8Memory(index, &mem))
    result->SetDouble(keys::kJsMemoryAllocatedKey,
        static_cast<double>(mem));

  if (model->GetV8MemoryUsed(index, &mem))
    result->SetDouble(keys::kJsMemoryUsedKey,
        static_cast<double>(mem));

  if (model->GetSqliteMemoryUsedBytes(index, &mem))
    result->SetDouble(keys::kSqliteMemoryKey,
        static_cast<double>(mem));

  WebKit::WebCache::ResourceTypeStats cache_stats;
  if (model->GetWebCoreCacheStats(index, &cache_stats)) {
    result->Set(keys::kImageCacheKey,
                CreateCacheData(cache_stats.images));
    result->Set(keys::kScriptCacheKey,
                CreateCacheData(cache_stats.scripts));
    result->Set(keys::kCssCacheKey,
                CreateCacheData(cache_stats.cssStyleSheets));
  }

  // Network and FPS are reported by the TaskManager per resource (tab), not
  // per process, therefore we need to iterate through the group of resources
  // and aggregate the data.
  float fps = 0, tmp = 0;
  int64 net = 0;
  int length = model->GetGroupRangeForResource(index).second;
  for (int i = 0; i < length; ++i) {
    net += model->GetNetworkUsage(index + i);
    if (model->GetFPS(index + i, &tmp))
      fps += tmp;
  }
  result->SetDouble(keys::kFPSKey, static_cast<double>(fps));
  result->SetDouble(keys::kNetworkKey, static_cast<double>(net));

  return result;
}

// Since memory details are expensive to gather, we don't do it by default.
// This function is a helper to add memory details data to an existing
// Process object representation.
void AddMemoryDetails(DictionaryValue* result,
                      TaskManagerModel* model,
                      int index) {
  size_t mem;
  int64 pr_mem = model->GetPrivateMemory(index, &mem) ?
      static_cast<int64>(mem) : -1;
  result->SetDouble(keys::kPrivateMemoryKey, static_cast<double>(pr_mem));
}

#endif  // defined(ENABLE_TASK_MANAGER)

} // local namespace

ProcessesEventRouter* ProcessesEventRouter::GetInstance() {
  return Singleton<ProcessesEventRouter>::get();
}

ProcessesEventRouter::ProcessesEventRouter()
    : listeners_(0),
      task_manager_listening_(false) {
#if defined(ENABLE_TASK_MANAGER)
  model_ = TaskManager::GetInstance()->model();
  model_->AddObserver(this);

  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_HANG,
      content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
      content::NotificationService::AllSources());
#endif  // defined(ENABLE_TASK_MANAGER)
}

ProcessesEventRouter::~ProcessesEventRouter() {
#if defined(ENABLE_TASK_MANAGER)
  registrar_.Remove(this, content::NOTIFICATION_RENDERER_PROCESS_HANG,
      content::NotificationService::AllSources());
  registrar_.Remove(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
      content::NotificationService::AllSources());

  if (task_manager_listening_)
    model_->StopListening();

  model_->RemoveObserver(this);
#endif  // defined(ENABLE_TASK_MANAGER)
}

void ProcessesEventRouter::ObserveProfile(Profile* profile) {
  profiles_.insert(profile);
}

void ProcessesEventRouter::ListenerAdded() {
#if defined(ENABLE_TASK_MANAGER)
  // The task manager has its own ref count to balance other callers of
  // StartUpdating/StopUpdating.
  model_->StartUpdating();
#endif  // defined(ENABLE_TASK_MANAGER)
  ++listeners_;
}

void ProcessesEventRouter::ListenerRemoved() {
  DCHECK(listeners_ > 0);
  --listeners_;
#if defined(ENABLE_TASK_MANAGER)
  // The task manager has its own ref count to balance other callers of
  // StartUpdating/StopUpdating.
  model_->StopUpdating();
#endif  // defined(ENABLE_TASK_MANAGER)
}

void ProcessesEventRouter::StartTaskManagerListening() {
#if defined(ENABLE_TASK_MANAGER)
  if (!task_manager_listening_) {
    model_->StartListening();
    task_manager_listening_ = true;
  }
#endif  // defined(ENABLE_TASK_MANAGER)
}

void ProcessesEventRouter::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {

  switch (type) {
    case content::NOTIFICATION_RENDERER_PROCESS_HANG:
      ProcessHangEvent(
          content::Source<content::RenderWidgetHost>(source).ptr());
      break;
    case content::NOTIFICATION_RENDERER_PROCESS_CLOSED:
      ProcessClosedEvent(
          content::Source<content::RenderProcessHost>(source).ptr(),
          content::Details<content::RenderProcessHost::RendererClosedDetails>(
              details).ptr());
      break;
    default:
      NOTREACHED() << "Unexpected observe of type " << type;
  }
  return;
}

void ProcessesEventRouter::OnItemsAdded(int start, int length) {
#if defined(ENABLE_TASK_MANAGER)
  DCHECK_EQ(length, 1);
  int index = start;

  std::string event(keys::kOnCreated);
  if (!HasEventListeners(event))
    return;

  // If the item being added is not the first one in the group, find the base
  // index and use it for retrieving the process data.
  if (!model_->IsResourceFirstInGroup(start)) {
    index = model_->GetGroupIndexForResource(start);
  }

  scoped_ptr<ListValue> args(new ListValue());
  DictionaryValue* process = CreateProcessFromModel(
      model_->GetUniqueChildProcessId(index), model_, index, false);
  DCHECK(process != NULL);

  if (process == NULL)
    return;

  args->Append(process);

  NotifyProfiles(keys::kOnCreated, args.Pass());
#endif  // defined(ENABLE_TASK_MANAGER)
}

void ProcessesEventRouter::OnItemsChanged(int start, int length) {
#if defined(ENABLE_TASK_MANAGER)
  // If we don't have any listeners, return immediately.
  if (listeners_ == 0)
    return;

  if (!model_)
    return;

  // We need to know which type of onUpdated events to fire and whether to
  // collect memory or not.
  std::string updated_event(keys::kOnUpdated);
  std::string updated_event_memory(keys::kOnUpdatedWithMemory);
  bool updated = HasEventListeners(updated_event);
  bool updated_memory = HasEventListeners(updated_event_memory);

  DCHECK(updated || updated_memory);

  IDMap<DictionaryValue> processes_map;
  for (int i = start; i < start + length; i++) {
    if (model_->IsResourceFirstInGroup(i)) {
      int id = model_->GetUniqueChildProcessId(i);
      DictionaryValue* process = CreateProcessFromModel(id, model_, i, true);
      processes_map.AddWithID(process, i);
    }
  }

  int id;
  std::string idkey(keys::kIdKey);
  DictionaryValue* processes = new DictionaryValue();

  if (updated) {
    IDMap<DictionaryValue>::iterator it(&processes_map);
    for (; !it.IsAtEnd(); it.Advance()) {
      if (!it.GetCurrentValue()->GetInteger(idkey, &id))
        continue;

      // Store each process indexed by the string version of its id.
      processes->Set(base::IntToString(id), it.GetCurrentValue());
    }

    scoped_ptr<ListValue> args(new ListValue());
    args->Append(processes);
    NotifyProfiles(keys::kOnUpdated, args.Pass());
  }

  if (updated_memory) {
    IDMap<DictionaryValue>::iterator it(&processes_map);
    for (; !it.IsAtEnd(); it.Advance()) {
      if (!it.GetCurrentValue()->GetInteger(idkey, &id))
        continue;

      AddMemoryDetails(it.GetCurrentValue(), model_, it.GetCurrentKey());

      // Store each process indexed by the string version of its id if we didn't
      // already insert it as part of the onUpdated processing above.
      if (!updated)
        processes->Set(base::IntToString(id), it.GetCurrentValue());
    }

    scoped_ptr<ListValue> args(new ListValue());
    args->Append(processes);
    NotifyProfiles(keys::kOnUpdatedWithMemory, args.Pass());
  }
#endif  // defined(ENABLE_TASK_MANAGER)
}

void ProcessesEventRouter::OnItemsToBeRemoved(int start, int length) {
#if defined(ENABLE_TASK_MANAGER)
  DCHECK(length == 1);

  // Process exit for renderer processes has the data about exit code and
  // termination status, therefore we will rely on notifications and not on
  // the Task Manager data. We do use the rest of this method for non-renderer
  // processes.
  if (model_->GetResourceType(start) == TaskManager::Resource::RENDERER)
    return;

  // The callback function parameters.
  scoped_ptr<ListValue> args(new ListValue());

  // First arg: The id of the process that was closed.
  args->Append(Value::CreateIntegerValue(
      model_->GetUniqueChildProcessId(start)));

  // Second arg: The exit type for the process.
  args->Append(Value::CreateIntegerValue(0));

  // Third arg: The exit code for the process.
  args->Append(Value::CreateIntegerValue(0));

  NotifyProfiles(keys::kOnExited, args.Pass());
#endif  // defined(ENABLE_TASK_MANAGER)
}

void ProcessesEventRouter::ProcessHangEvent(content::RenderWidgetHost* widget) {
#if defined(ENABLE_TASK_MANAGER)
  std::string event(keys::kOnUnresponsive);
  if (!HasEventListeners(event))
    return;

  DictionaryValue* process = NULL;
  int count = model_->ResourceCount();
  int id = widget->GetProcess()->GetID();

  for (int i = 0; i < count; ++i) {
    if (model_->IsResourceFirstInGroup(i)) {
      if (id == model_->GetUniqueChildProcessId(i)) {
        process = CreateProcessFromModel(id, model_, i, false);
        break;
      }
    }
  }

  DCHECK(process);
  if (process == NULL)
    return;

  scoped_ptr<ListValue> args(new ListValue());
  args->Append(process);

  NotifyProfiles(keys::kOnUnresponsive, args.Pass());
#endif  // defined(ENABLE_TASK_MANAGER)
}

void ProcessesEventRouter::ProcessClosedEvent(
    content::RenderProcessHost* rph,
    content::RenderProcessHost::RendererClosedDetails* details) {
#if defined(ENABLE_TASK_MANAGER)
  // The callback function parameters.
  scoped_ptr<ListValue> args(new ListValue());

  // First arg: The id of the process that was closed.
  args->Append(Value::CreateIntegerValue(rph->GetID()));

  // Second arg: The exit type for the process.
  args->Append(Value::CreateIntegerValue(details->status));

  // Third arg: The exit code for the process.
  args->Append(Value::CreateIntegerValue(details->exit_code));

  NotifyProfiles(keys::kOnExited, args.Pass());
#endif  // defined(ENABLE_TASK_MANAGER)
}

void ProcessesEventRouter::DispatchEvent(Profile* profile,
                                         const char* event_name,
                                         scoped_ptr<ListValue> event_args) {
  if (profile && profile->GetExtensionEventRouter()) {
    profile->GetExtensionEventRouter()->DispatchEventToRenderers(
        event_name, event_args.Pass(), NULL, GURL(),
        extensions::EventFilteringInfo());
  }
}

void ProcessesEventRouter::NotifyProfiles(const char* event_name,
                                          scoped_ptr<ListValue> event_args) {
  for (ProfileSet::iterator it = profiles_.begin();
       it != profiles_.end(); it++) {
    Profile* profile = *it;
    scoped_ptr<ListValue> event_args_copy(event_args->DeepCopy());
    DispatchEvent(profile, event_name, event_args_copy.Pass());
  }
}

// In order to determine whether there are any listeners for the event of
// interest, we need to ask each profile whether it has one registered.
// We only need to look for the profiles that have registered with the
// this extension API.
bool ProcessesEventRouter::HasEventListeners(std::string& event_name) {
  for (ProfileSet::iterator it = profiles_.begin();
       it != profiles_.end(); it++) {
    Profile* profile = *it;
    extensions::EventRouter* router = profile->GetExtensionEventRouter();
    if (!router)
      continue;

    if (router->HasEventListener(event_name))
      return true;
  }
  return false;
}

bool GetProcessIdForTabFunction::RunImpl() {
#if defined(ENABLE_TASK_MANAGER)
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &tab_id_));

  // Add a reference, which is balanced in GetProcessIdForTab to keep the object
  // around and allow for the callback to be invoked.
  AddRef();

  // If the task manager is already listening, just post a task to execute
  // which will invoke the callback once we have returned from this function.
  // Otherwise, wait for the notification that the task manager is done with
  // the data gathering.
  if (ProcessesEventRouter::GetInstance()->is_task_manager_listening()) {
    MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
        &GetProcessIdForTabFunction::GetProcessIdForTab, this));
  } else {
    registrar_.Add(this,
                   chrome::NOTIFICATION_TASK_MANAGER_CHILD_PROCESSES_DATA_READY,
                   content::NotificationService::AllSources());
    ProcessesEventRouter::GetInstance()->StartTaskManagerListening();
  }

  return true;
#else
  error_ = errors::kExtensionNotSupported;
  return false;
#endif  // defined(ENABLE_TASK_MANAGER)
}

void GetProcessIdForTabFunction::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_TASK_MANAGER_CHILD_PROCESSES_DATA_READY);
  registrar_.RemoveAll();
  GetProcessIdForTab();
}

void GetProcessIdForTabFunction::GetProcessIdForTab() {
  TabContents* contents = NULL;
  int tab_index = -1;
  if (!ExtensionTabUtil::GetTabById(tab_id_, profile(), include_incognito(),
                                    NULL, NULL, &contents, &tab_index)) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(
        extensions::tabs_constants::kTabNotFoundError,
        base::IntToString(tab_id_));
    SetResult(Value::CreateIntegerValue(-1));
    SendResponse(false);
  } else {
    int process_id = contents->web_contents()->GetRenderProcessHost()->GetID();
    SetResult(Value::CreateIntegerValue(process_id));
    SendResponse(true);
  }

  // Balance the AddRef in the RunImpl.
  Release();
}

bool TerminateFunction::RunImpl() {
#if defined(ENABLE_TASK_MANAGER)
  EXTENSION_FUNCTION_VALIDATE(args_->GetInteger(0, &process_id_));

  // Add a reference, which is balanced in TerminateProcess to keep the object
  // around and allow for the callback to be invoked.
  AddRef();

  // If the task manager is already listening, just post a task to execute
  // which will invoke the callback once we have returned from this function.
  // Otherwise, wait for the notification that the task manager is done with
  // the data gathering.
  if (ProcessesEventRouter::GetInstance()->is_task_manager_listening()) {
    MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
        &TerminateFunction::TerminateProcess, this));
  } else {
    registrar_.Add(this,
                   chrome::NOTIFICATION_TASK_MANAGER_CHILD_PROCESSES_DATA_READY,
                   content::NotificationService::AllSources());
    ProcessesEventRouter::GetInstance()->StartTaskManagerListening();
  }

  return true;
#else
  error_ = errors::kExtensionNotSupported;
  return false;
#endif  // defined(ENABLE_TASK_MANAGER)
}

void TerminateFunction::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_TASK_MANAGER_CHILD_PROCESSES_DATA_READY);
  registrar_.RemoveAll();
  TerminateProcess();
}

void TerminateFunction::TerminateProcess() {
  TaskManagerModel* model = TaskManager::GetInstance()->model();

  int count = model->ResourceCount();
  bool killed = false;
  bool found = false;

  for (int i = 0; i < count; ++i) {
    if (model->IsResourceFirstInGroup(i)) {
      if (process_id_ == model->GetUniqueChildProcessId(i)) {
        found = true;
        killed = base::KillProcess(model->GetProcess(i),
            content::RESULT_CODE_KILLED, true);
        UMA_HISTOGRAM_COUNTS("ChildProcess.KilledByExtensionAPI", 1);
        break;
      }
    }
  }

  if (!found) {
    error_ = ExtensionErrorUtils::FormatErrorMessage(errors::kProcessNotFound,
        base::IntToString(process_id_));
    SendResponse(false);
  } else {
    SetResult(Value::CreateBooleanValue(killed));
    SendResponse(true);
  }

  // Balance the AddRef in the RunImpl.
  Release();
}

GetProcessInfoFunction::GetProcessInfoFunction()
#if defined(ENABLE_TASK_MANAGER)
  : memory_(false)
#endif
    {
}

GetProcessInfoFunction::~GetProcessInfoFunction() {
}

bool GetProcessInfoFunction::RunImpl() {
#if defined(ENABLE_TASK_MANAGER)
  Value* processes = NULL;

  EXTENSION_FUNCTION_VALIDATE(args_->Get(0, &processes));
  EXTENSION_FUNCTION_VALIDATE(args_->GetBoolean(1, &memory_));

  EXTENSION_FUNCTION_VALIDATE(extensions::ReadOneOrMoreIntegers(
      processes, &process_ids_));

  // Add a reference, which is balanced in GatherProcessInfo to keep the object
  // around and allow for the callback to be invoked.
  AddRef();

  // If the task manager is already listening, just post a task to execute
  // which will invoke the callback once we have returned from this function.
  // Otherwise, wait for the notification that the task manager is done with
  // the data gathering.
  if (ProcessesEventRouter::GetInstance()->is_task_manager_listening()) {
    MessageLoop::current()->PostTask(FROM_HERE, base::Bind(
        &GetProcessInfoFunction::GatherProcessInfo, this));
  } else {
    registrar_.Add(this,
                   chrome::NOTIFICATION_TASK_MANAGER_CHILD_PROCESSES_DATA_READY,
                   content::NotificationService::AllSources());
    ProcessesEventRouter::GetInstance()->StartTaskManagerListening();
  }
  return true;

#else
  error_ = errors::kExtensionNotSupported;
  return false;
#endif  // defined(ENABLE_TASK_MANAGER)
}

void GetProcessInfoFunction::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_TASK_MANAGER_CHILD_PROCESSES_DATA_READY);
  registrar_.RemoveAll();
  GatherProcessInfo();
}

void GetProcessInfoFunction::GatherProcessInfo() {
#if defined(ENABLE_TASK_MANAGER)
  TaskManagerModel* model = TaskManager::GetInstance()->model();
  DictionaryValue* processes = new DictionaryValue();

  // If there are no process IDs specified, it means we need to return all of
  // the ones we know of.
  if (process_ids_.size() == 0) {
    int resources = model->ResourceCount();
    for (int i = 0; i < resources; ++i) {
      if (model->IsResourceFirstInGroup(i)) {
        int id = model->GetUniqueChildProcessId(i);
        DictionaryValue* d = CreateProcessFromModel(id, model, i, false);
        if (memory_)
          AddMemoryDetails(d, model, i);
        processes->Set(base::IntToString(id), d);
      }
    }
  } else {
    int resources = model->ResourceCount();
    for (int i = 0; i < resources; ++i) {
      if (model->IsResourceFirstInGroup(i)) {
        int id = model->GetUniqueChildProcessId(i);
        std::vector<int>::iterator proc_id = std::find(process_ids_.begin(),
                                                       process_ids_.end(), id);
        if (proc_id != process_ids_.end()) {
          DictionaryValue* d = CreateProcessFromModel(id, model, i, false);
          if (memory_)
            AddMemoryDetails(d, model, i);
          processes->Set(base::IntToString(id), d);

          process_ids_.erase(proc_id);
          if (process_ids_.size() == 0)
            break;
        }
      }
    }
    DCHECK(process_ids_.size() == 0);
  }

  SetResult(processes);
  SendResponse(true);

  // Balance the AddRef in the RunImpl.
  Release();
#endif  // defined(ENABLE_TASK_MANAGER)
}

}  // namespace extensions
