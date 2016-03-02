// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/processes/processes_api.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <utility>

#include "base/callback.h"
#include "base/json/json_writer.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/metrics/histogram.h"
#include "base/single_thread_task_runner.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "base/values.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/extensions/api/tabs/tabs_constants.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/extensions/extension_tab_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/task_manager/resource_provider.h"
#include "chrome/browser/task_manager/task_manager.h"
#include "chrome/common/extensions/api/processes.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host.h"
#include "content/public/browser/render_widget_host_iterator.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/result_codes.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/browser/extension_function_util.h"
#include "extensions/common/error_utils.h"

namespace extensions {

namespace errors {
const char kNotAllowedToTerminate[] = "Not allowed to terminate process: *.";
const char kProcessNotFound[] = "Process not found: *.";
const char kInavlidArgument[] = "Invalid argument: *.";
}  // namespace errors

namespace {

#if defined(ENABLE_TASK_MANAGER)

scoped_ptr<api::processes::Cache> CreateCacheData(
    const blink::WebCache::ResourceTypeStat& stat) {
  scoped_ptr<api::processes::Cache> cache(new api::processes::Cache());
  cache->size = static_cast<double>(stat.size);
  cache->live_size = static_cast<double>(stat.liveSize);
  return cache;
}

api::processes::ProcessType GetProcessType(TaskManagerModel* model,
                                           int index) {
  // Determine process type.
  task_manager::Resource::Type resource_type = model->GetResourceType(index);
  switch (resource_type) {
    case task_manager::Resource::BROWSER:
      return api::processes::PROCESS_TYPE_BROWSER;

    case task_manager::Resource::RENDERER:
      return api::processes::PROCESS_TYPE_RENDERER;

    case task_manager::Resource::EXTENSION:
    case task_manager::Resource::GUEST:
      return api::processes::PROCESS_TYPE_EXTENSION;

    case task_manager::Resource::NOTIFICATION:
      return api::processes::PROCESS_TYPE_NOTIFICATION;

    case task_manager::Resource::PLUGIN:
      return api::processes::PROCESS_TYPE_PLUGIN;

    case task_manager::Resource::WORKER:
      return api::processes::PROCESS_TYPE_WORKER;

    case task_manager::Resource::NACL:
      return api::processes::PROCESS_TYPE_NACL;

    case task_manager::Resource::UTILITY:
      return api::processes::PROCESS_TYPE_UTILITY;

    case task_manager::Resource::GPU:
      return api::processes::PROCESS_TYPE_GPU;

    case task_manager::Resource::ZYGOTE:
    case task_manager::Resource::SANDBOX_HELPER:
    case task_manager::Resource::UNKNOWN:
      return api::processes::PROCESS_TYPE_OTHER;
  }

  NOTREACHED() << "Unknown resource type.";
  return api::processes::PROCESS_TYPE_NONE;
}

void FillTabsForProcess(int process_id, api::processes::Process* out_process) {
  DCHECK(out_process);

  // The tabs list only makes sense for render processes, so if we don't find
  // one, just return the empty list.
  content::RenderProcessHost* rph =
      content::RenderProcessHost::FromID(process_id);
  if (!rph)
    return;

  int tab_id = -1;
  // We need to loop through all the RVHs to ensure we collect the set of all
  // tabs using this renderer process.
  scoped_ptr<content::RenderWidgetHostIterator> widgets(
      content::RenderWidgetHost::GetRenderWidgetHosts());
  while (content::RenderWidgetHost* widget = widgets->GetNextHost()) {
    if (widget->GetProcess()->GetID() != process_id)
      continue;

    content::RenderViewHost* host = content::RenderViewHost::From(widget);
    content::WebContents* contents =
        content::WebContents::FromRenderViewHost(host);
    if (contents) {
      tab_id = ExtensionTabUtil::GetTabId(contents);
      if (tab_id != -1)
        out_process->tabs.push_back(tab_id);
    }
  }
}

// This function fills |out_process| with the data of the process with
// |process_id|. For memory details, which are not added by this function,
// the callers need to use AddMemoryDetails.
void FillProcessData(int process_id,
                     TaskManagerModel* model,
                     int index,
                     bool include_optional,
                     api::processes::Process* out_process) {
  DCHECK(out_process);

  out_process->id = process_id;
  out_process->os_process_id = model->GetProcessId(index);
  out_process->type = GetProcessType(model, index);
  out_process->title = base::UTF16ToUTF8(model->GetResourceTitle(index));
  out_process->profile =
      base::UTF16ToUTF8(model->GetResourceProfileName(index));
  out_process->nacl_debug_port = model->GetNaClDebugStubPort(index);

  FillTabsForProcess(process_id, out_process);

  // If we don't need to include the optional properties, just return now.
  if (!include_optional)
    return;

  out_process->cpu.reset(new double(model->GetCPUUsage(index)));

  size_t mem = 0;
  if (model->GetV8Memory(index, &mem)) {
    out_process->js_memory_allocated.reset(new double(static_cast<double>(
        mem)));
  }

  if (model->GetV8MemoryUsed(index, &mem))
    out_process->js_memory_used.reset(new double(static_cast<double>(mem)));

  if (model->GetSqliteMemoryUsedBytes(index, &mem))
    out_process->sqlite_memory.reset(new double(static_cast<double>(mem)));

  blink::WebCache::ResourceTypeStats cache_stats;
  if (model->GetWebCoreCacheStats(index, &cache_stats)) {
    out_process->image_cache = CreateCacheData(cache_stats.images);
    out_process->script_cache = CreateCacheData(cache_stats.scripts);
    out_process->css_cache = CreateCacheData(cache_stats.cssStyleSheets);
  }

  // Network is reported by the TaskManager per resource (tab), not per
  // process, therefore we need to iterate through the group of resources
  // and aggregate the data.
  int64_t net = 0;
  int length = model->GetGroupRangeForResource(index).second;
  for (int i = 0; i < length; ++i)
    net += model->GetNetworkUsage(index + i);
  out_process->network.reset(new double(static_cast<double>(net)));
}

// Since memory details are expensive to gather, we don't do it by default.
// This function is a helper to add memory details data to an existing
// Process object |out_process|.
void AddMemoryDetails(TaskManagerModel* model,
                      int index,
                      api::processes::Process* out_process) {
  DCHECK(out_process);

  size_t mem;
  int64_t pr_mem =
      model->GetPrivateMemory(index, &mem) ? static_cast<int64_t>(mem) : -1;
  out_process->private_memory.reset(new double(static_cast<double>(pr_mem)));
}

#endif  // defined(ENABLE_TASK_MANAGER)

}  // namespace

ProcessesEventRouter::ProcessesEventRouter(content::BrowserContext* context)
    : browser_context_(context), listeners_(0), task_manager_listening_(false) {
#if defined(ENABLE_TASK_MANAGER)
  model_ = TaskManager::GetInstance()->model();
  model_->AddObserver(this);

  registrar_.Add(this, content::NOTIFICATION_RENDER_WIDGET_HOST_HANG,
      content::NotificationService::AllSources());
  registrar_.Add(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
      content::NotificationService::AllSources());
#endif  // defined(ENABLE_TASK_MANAGER)
}

ProcessesEventRouter::~ProcessesEventRouter() {
#if defined(ENABLE_TASK_MANAGER)
  registrar_.Remove(this, content::NOTIFICATION_RENDER_WIDGET_HOST_HANG,
      content::NotificationService::AllSources());
  registrar_.Remove(this, content::NOTIFICATION_RENDERER_PROCESS_CLOSED,
      content::NotificationService::AllSources());

  if (task_manager_listening_)
    model_->StopListening();

  model_->RemoveObserver(this);
#endif  // defined(ENABLE_TASK_MANAGER)
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
  DCHECK_GT(listeners_, 0);
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
    case content::NOTIFICATION_RENDER_WIDGET_HOST_HANG:
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
  if (!HasEventListeners(api::processes::OnCreated::kEventName))
    return;

  // If the item being added is not the first one in the group, find the base
  // index and use it for retrieving the process data.
  if (!model_->IsResourceFirstInGroup(start))
    start = model_->GetGroupIndexForResource(start);

  api::processes::Process process;
  FillProcessData(model_->GetUniqueChildProcessId(start), model_, start,
                  false /* include_optional */, &process);
  DispatchEvent(events::PROCESSES_ON_CREATED,
                api::processes::OnCreated::kEventName,
                api::processes::OnCreated::Create(process));
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
  bool updated = HasEventListeners(api::processes::OnUpdated::kEventName);
  bool updated_memory =
      HasEventListeners(api::processes::OnUpdatedWithMemory::kEventName);
  if (!updated && !updated_memory)
    return;

  base::DictionaryValue processes_dictionary;
  for (int i = start; i < start + length; ++i) {
    if (model_->IsResourceFirstInGroup(i)) {
      int id = model_->GetUniqueChildProcessId(i);
      api::processes::Process process;
      FillProcessData(id, model_, i, true /* include_optional */, &process);
      if (updated_memory)
        AddMemoryDetails(model_, i, &process);
      processes_dictionary.Set(base::IntToString(id), process.ToValue());
    }
  }

  if (updated) {
    api::processes::OnUpdated::Processes processes;
    processes.additional_properties.MergeDictionary(&processes_dictionary);
    // NOTE: If there are listeners to the updates with memory as well,
    // listeners to onUpdated (without memory) will also get the memory info
    // of processes as an added bonus.
    DispatchEvent(events::PROCESSES_ON_UPDATED,
                  api::processes::OnUpdated::kEventName,
                  api::processes::OnUpdated::Create(processes));
  }

  if (updated_memory) {
    api::processes::OnUpdatedWithMemory::Processes processes;
    processes.additional_properties.MergeDictionary(&processes_dictionary);
    DispatchEvent(events::PROCESSES_ON_UPDATED_WITH_MEMORY,
                  api::processes::OnUpdatedWithMemory::kEventName,
                  api::processes::OnUpdatedWithMemory::Create(processes));}
#endif  // defined(ENABLE_TASK_MANAGER)
}

void ProcessesEventRouter::OnItemsToBeRemoved(int start, int length) {
#if defined(ENABLE_TASK_MANAGER)
  DCHECK_EQ(length, 1);

  if (!HasEventListeners(api::processes::OnExited::kEventName))
    return;

  // Process exit for renderer processes has the data about exit code and
  // termination status, therefore we will rely on notifications and not on
  // the Task Manager data. We do use the rest of this method for non-renderer
  // processes.
  if (model_->GetResourceType(start) == task_manager::Resource::RENDERER)
    return;

  DispatchEvent(events::PROCESSES_ON_EXITED,
                api::processes::OnExited::kEventName,
                api::processes::OnExited::Create(
                    model_->GetUniqueChildProcessId(start),
                    0 /* exit_type */,
                    0 /* exit_code */));
#endif  // defined(ENABLE_TASK_MANAGER)
}

void ProcessesEventRouter::ProcessHangEvent(content::RenderWidgetHost* widget) {
#if defined(ENABLE_TASK_MANAGER)
  if (!HasEventListeners(api::processes::OnUnresponsive::kEventName))
    return;

  int count = model_->ResourceCount();
  int id = widget->GetProcess()->GetID();

  for (int i = 0; i < count; ++i) {
    if (model_->IsResourceFirstInGroup(i)) {
      if (id == model_->GetUniqueChildProcessId(i)) {
        api::processes::Process process;
        FillProcessData(id, model_, i, false /* include_optional */, &process);
        DispatchEvent(events::PROCESSES_ON_UNRESPONSIVE,
                      api::processes::OnUnresponsive::kEventName,
                      api::processes::OnUnresponsive::Create(process));
        return;
      }
    }
  }
#endif  // defined(ENABLE_TASK_MANAGER)
}

void ProcessesEventRouter::ProcessClosedEvent(
    content::RenderProcessHost* rph,
    content::RenderProcessHost::RendererClosedDetails* details) {
#if defined(ENABLE_TASK_MANAGER)
  if (!HasEventListeners(api::processes::OnExited::kEventName))
    return;

  DispatchEvent(events::PROCESSES_ON_EXITED,
                api::processes::OnExited::kEventName,
                api::processes::OnExited::Create(rph->GetID(),
                                                 details->status,
                                                 details->exit_code));
#endif  // defined(ENABLE_TASK_MANAGER)
}

void ProcessesEventRouter::DispatchEvent(
    events::HistogramValue histogram_value,
    const std::string& event_name,
    scoped_ptr<base::ListValue> event_args) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  if (event_router) {
    scoped_ptr<Event> event(
        new Event(histogram_value, event_name, std::move(event_args)));
    event_router->BroadcastEvent(std::move(event));
  }
}

bool ProcessesEventRouter::HasEventListeners(const std::string& event_name) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  return event_router && event_router->HasEventListener(event_name);
}

ProcessesAPI::ProcessesAPI(content::BrowserContext* context)
    : browser_context_(context) {
  EventRouter* event_router = EventRouter::Get(browser_context_);
  // Monitor when the following events are being listened to in order to know
  // when to start the task manager.
  event_router->RegisterObserver(this, api::processes::OnUpdated::kEventName);
  event_router->RegisterObserver(
      this, api::processes::OnUpdatedWithMemory::kEventName);
  event_router->RegisterObserver(this, api::processes::OnCreated::kEventName);
  event_router->RegisterObserver(this,
                                 api::processes::OnUnresponsive::kEventName);
  event_router->RegisterObserver(this, api::processes::OnExited::kEventName);
}

ProcessesAPI::~ProcessesAPI() {
}

void ProcessesAPI::Shutdown() {
  EventRouter::Get(browser_context_)->UnregisterObserver(this);
}

static base::LazyInstance<BrowserContextKeyedAPIFactory<ProcessesAPI> >
    g_factory = LAZY_INSTANCE_INITIALIZER;

// static
BrowserContextKeyedAPIFactory<ProcessesAPI>*
ProcessesAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

// static
ProcessesAPI* ProcessesAPI::Get(content::BrowserContext* context) {
  return BrowserContextKeyedAPIFactory<ProcessesAPI>::Get(context);
}

ProcessesEventRouter* ProcessesAPI::processes_event_router() {
  if (!processes_event_router_)
    processes_event_router_.reset(new ProcessesEventRouter(browser_context_));
  return processes_event_router_.get();
}

void ProcessesAPI::OnListenerAdded(const EventListenerInfo& details) {
  // We lazily tell the TaskManager to start updating when listeners to the
  // processes.onUpdated or processes.onUpdatedWithMemory events arrive.
  processes_event_router()->ListenerAdded();
}

void ProcessesAPI::OnListenerRemoved(const EventListenerInfo& details) {
  // If a processes.onUpdated or processes.onUpdatedWithMemory event listener
  // is removed (or a process with one exits), then we let the extension API
  // know that it has one fewer listener.
  processes_event_router()->ListenerRemoved();
}

////////////////////////////////////////////////////////////////////////////////
// ProcessesGetProcessIdForTabFunction:
////////////////////////////////////////////////////////////////////////////////

ProcessesGetProcessIdForTabFunction::ProcessesGetProcessIdForTabFunction()
    : tab_id_(-1) {
}

ExtensionFunction::ResponseAction ProcessesGetProcessIdForTabFunction::Run() {
#if defined(ENABLE_TASK_MANAGER)
  scoped_ptr<api::processes::GetProcessIdForTab::Params> params(
  api::processes::GetProcessIdForTab::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  tab_id_ = params->tab_id;

  if (tab_id_ < 0) {
    return RespondNow(Error(errors::kInavlidArgument,
                            base::IntToString(tab_id_)));
  }

  // Add a reference, which is balanced in GetProcessIdForTab to keep the object
  // around and allow for the callback to be invoked.
  AddRef();

  // If the task manager is already listening, just post a task to execute
  // which will invoke the callback once we have returned from this function.
  // Otherwise, wait for the notification that the task manager is done with
  // the data gathering.
  if (ProcessesAPI::Get(Profile::FromBrowserContext(browser_context()))
          ->processes_event_router()
          ->is_task_manager_listening()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&ProcessesGetProcessIdForTabFunction::GetProcessIdForTab,
                   this));
  } else {
    TaskManager::GetInstance()->model()->RegisterOnDataReadyCallback(
        base::Bind(&ProcessesGetProcessIdForTabFunction::GetProcessIdForTab,
                   this));

    ProcessesAPI::Get(Profile::FromBrowserContext(browser_context()))
        ->processes_event_router()
        ->StartTaskManagerListening();
  }

  return RespondLater();
#else
  return RespondNow(Error(errors::kExtensionNotSupported));
#endif  // defined(ENABLE_TASK_MANAGER)
}

void ProcessesGetProcessIdForTabFunction::GetProcessIdForTab() {
  content::WebContents* contents = NULL;
  int tab_index = -1;
  if (!ExtensionTabUtil::GetTabById(
          tab_id_,
          Profile::FromBrowserContext(browser_context()),
          include_incognito(),
          nullptr,
          nullptr,
          &contents,
          &tab_index)) {
    Respond(Error(tabs_constants::kTabNotFoundError,
                  base::IntToString(tab_id_)));
  } else {
    int process_id = contents->GetRenderProcessHost()->GetID();
    Respond(ArgumentList(
        api::processes::GetProcessIdForTab::Results::Create(process_id)));
  }

  // Balance the AddRef in the Run.
  Release();
}

////////////////////////////////////////////////////////////////////////////////
// ProcessesTerminateFunction
////////////////////////////////////////////////////////////////////////////////

ProcessesTerminateFunction::ProcessesTerminateFunction() : process_id_(-1) {
}

ExtensionFunction::ResponseAction ProcessesTerminateFunction::Run() {
#if defined(ENABLE_TASK_MANAGER)
  scoped_ptr<api::processes::Terminate::Params> params(
      api::processes::Terminate::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  process_id_ = params->process_id;

  // Add a reference, which is balanced in TerminateProcess to keep the object
  // around and allow for the callback to be invoked.
  AddRef();

  // If the task manager is already listening, just post a task to execute
  // which will invoke the callback once we have returned from this function.
  // Otherwise, wait for the notification that the task manager is done with
  // the data gathering.
  if (ProcessesAPI::Get(Profile::FromBrowserContext(browser_context()))
          ->processes_event_router()
          ->is_task_manager_listening()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&ProcessesTerminateFunction::TerminateProcess,
                              this));
  } else {
    TaskManager::GetInstance()->model()->RegisterOnDataReadyCallback(
        base::Bind(&ProcessesTerminateFunction::TerminateProcess, this));

    ProcessesAPI::Get(Profile::FromBrowserContext(browser_context()))
        ->processes_event_router()
        ->StartTaskManagerListening();
  }

  return RespondLater();
#else
  return RespondNow(Error(errors::kExtensionNotSupported));
#endif  // defined(ENABLE_TASK_MANAGER)
}

void ProcessesTerminateFunction::TerminateProcess() {
#if defined(ENABLE_TASK_MANAGER)
  TaskManagerModel* model = TaskManager::GetInstance()->model();

  bool found = false;
  for (int i = 0, count = model->ResourceCount(); i < count; ++i) {
    if (model->IsResourceFirstInGroup(i) &&
        process_id_ == model->GetUniqueChildProcessId(i)) {
      base::ProcessHandle process_handle = model->GetProcess(i);
      if (process_handle == base::GetCurrentProcessHandle()) {
        // Cannot kill the browser process.
        // TODO(kalman): Are there other sensitive processes?
        Respond(Error(errors::kNotAllowedToTerminate,
                      base::IntToString(process_id_)));
      } else {
        base::Process process =
            base::Process::DeprecatedGetProcessFromHandle(process_handle);
        bool did_terminate =
            process.Terminate(content::RESULT_CODE_KILLED, true);
        if (did_terminate)
          UMA_HISTOGRAM_COUNTS("ChildProcess.KilledByExtensionAPI", 1);

        Respond(ArgumentList(
            api::processes::Terminate::Results::Create(did_terminate)));
      }
      found = true;
      break;
    }
  }

  if (!found)
    Respond(Error(errors::kProcessNotFound, base::IntToString(process_id_)));

  // Balance the AddRef in the Run.
  Release();
#endif  // defined(ENABLE_TASK_MANAGER)
}

////////////////////////////////////////////////////////////////////////////////
// ProcessesGetProcessInfoFunction
////////////////////////////////////////////////////////////////////////////////

ProcessesGetProcessInfoFunction::ProcessesGetProcessInfoFunction()
#if defined(ENABLE_TASK_MANAGER)
  : memory_(false)
#endif
    {
}

ExtensionFunction::ResponseAction ProcessesGetProcessInfoFunction::Run() {
#if defined(ENABLE_TASK_MANAGER)
  scoped_ptr<api::processes::GetProcessInfo::Params> params(
      api::processes::GetProcessInfo::Params::Create(*args_));
  EXTENSION_FUNCTION_VALIDATE(params.get());
  if (params->process_ids.as_integer)
    process_ids_.push_back(*params->process_ids.as_integer);
  else
    process_ids_.swap(*params->process_ids.as_integers);

  memory_ = params->include_memory;

  // Add a reference, which is balanced in GatherProcessInfo to keep the object
  // around and allow for the callback to be invoked.
  AddRef();

  // If the task manager is already listening, just post a task to execute
  // which will invoke the callback once we have returned from this function.
  // Otherwise, wait for the notification that the task manager is done with
  // the data gathering.
  if (ProcessesAPI::Get(Profile::FromBrowserContext(browser_context()))
          ->processes_event_router()
          ->is_task_manager_listening()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::Bind(&ProcessesGetProcessInfoFunction::GatherProcessInfo, this));
  } else {
    TaskManager::GetInstance()->model()->RegisterOnDataReadyCallback(
        base::Bind(&ProcessesGetProcessInfoFunction::GatherProcessInfo, this));

    ProcessesAPI::Get(Profile::FromBrowserContext(browser_context()))
        ->processes_event_router()
        ->StartTaskManagerListening();
  }

  return RespondLater();
#else
  return RespondNow(Error(errors::kExtensionNotSupported));
#endif  // defined(ENABLE_TASK_MANAGER)
}

ProcessesGetProcessInfoFunction::~ProcessesGetProcessInfoFunction() {
}

void ProcessesGetProcessInfoFunction::GatherProcessInfo() {
#if defined(ENABLE_TASK_MANAGER)
  TaskManagerModel* model = TaskManager::GetInstance()->model();
  api::processes::GetProcessInfo::Results::Processes processes;

  // If there are no process IDs specified, it means we need to return all of
  // the ones we know of.
  if (process_ids_.size() == 0) {
    int resources = model->ResourceCount();
    for (int i = 0; i < resources; ++i) {
      if (model->IsResourceFirstInGroup(i)) {
        int id = model->GetUniqueChildProcessId(i);
        api::processes::Process process;
        FillProcessData(id, model, i, false, &process);
        if (memory_)
          AddMemoryDetails(model, i, &process);
        processes.additional_properties.Set(base::IntToString(id),
                                            process.ToValue());
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
          api::processes::Process process;
          FillProcessData(id, model, i, false, &process);
          if (memory_)
            AddMemoryDetails(model, i, &process);
          processes.additional_properties.Set(base::IntToString(id),
                                              process.ToValue());

          process_ids_.erase(proc_id);
          if (process_ids_.size() == 0)
            break;
        }
      }
    }
    // If not all processes were found, log them to the extension's console to
    // help the developer, but don't fail the API call.
    for (int pid : process_ids_) {
      WriteToConsole(content::CONSOLE_MESSAGE_LEVEL_ERROR,
                     ErrorUtils::FormatErrorMessage(errors::kProcessNotFound,
                                                    base::IntToString(pid)));
    }
  }

  // Send the response.
  Respond(ArgumentList(
      api::processes::GetProcessInfo::Results::Create(processes)));

  // Balance the AddRef in the Run.
  Release();
#endif  // defined(ENABLE_TASK_MANAGER)
}

}  // namespace extensions
