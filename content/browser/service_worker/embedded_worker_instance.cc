// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/embedded_worker_instance.h"

#include <utility>

#include "base/bind_helpers.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/bad_message.h"
#include "content/browser/devtools/service_worker_devtools_manager.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/embedded_worker_status.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/common/content_switches_internal.h"
#include "content/common/service_worker/embedded_worker_messages.h"
#include "content/common/service_worker/embedded_worker_settings.h"
#include "content/common/service_worker/embedded_worker_start_params.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/content_browser_client.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/child_process_host.h"
#include "content/public/common/content_client.h"
#include "content/public/common/content_switches.h"
#include "ipc/ipc_message.h"
#include "third_party/WebKit/public/web/WebConsoleMessage.h"
#include "url/gurl.h"

namespace content {

namespace {

// When a service worker version's failure count exceeds
// |kMaxSameProcessFailureCount|, the embedded worker is forced to start in a
// new process.
const int kMaxSameProcessFailureCount = 2;

const char kServiceWorkerTerminationCanceledMesage[] =
    "Service Worker termination by a timeout timer was canceled because "
    "DevTools is attached.";

void NotifyWorkerReadyForInspectionOnUI(int worker_process_id,
                                        int worker_route_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ServiceWorkerDevToolsManager::GetInstance()->WorkerReadyForInspection(
      worker_process_id, worker_route_id);
}

void NotifyWorkerDestroyedOnUI(int worker_process_id, int worker_route_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ServiceWorkerDevToolsManager::GetInstance()->WorkerDestroyed(
      worker_process_id, worker_route_id);
}

void NotifyWorkerVersionInstalledOnUI(int worker_process_id,
                                      int worker_route_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ServiceWorkerDevToolsManager::GetInstance()->WorkerVersionInstalled(
      worker_process_id, worker_route_id);
}

void NotifyWorkerVersionDoomedOnUI(int worker_process_id, int worker_route_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  ServiceWorkerDevToolsManager::GetInstance()->WorkerVersionDoomed(
      worker_process_id, worker_route_id);
}

using SetupProcessCallback = base::OnceCallback<void(
    ServiceWorkerStatusCode,
    std::unique_ptr<EmbeddedWorkerStartParams>,
    std::unique_ptr<ServiceWorkerProcessManager::AllocatedProcessInfo>,
    std::unique_ptr<EmbeddedWorkerInstance::DevToolsProxy> devtools_proxy)>;

// Allocates a renderer process for starting a worker and does setup like
// registering with DevTools. Called on the UI thread. Calls |callback| on the
// IO thread. |context| and |weak_context| are only for passing to DevTools and
// must not be dereferenced here on the UI thread.
void SetupOnUIThread(base::WeakPtr<ServiceWorkerProcessManager> process_manager,
                     bool can_use_existing_process,
                     std::unique_ptr<EmbeddedWorkerStartParams> params,
                     mojom::EmbeddedWorkerInstanceClientRequest request,
                     ServiceWorkerContextCore* context,
                     base::WeakPtr<ServiceWorkerContextCore> weak_context,
                     SetupProcessCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  auto process_info =
      base::MakeUnique<ServiceWorkerProcessManager::AllocatedProcessInfo>();
  std::unique_ptr<EmbeddedWorkerInstance::DevToolsProxy> devtools_proxy;
  if (!process_manager) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(std::move(callback), SERVICE_WORKER_ERROR_ABORT,
                       std::move(params), std::move(process_info),
                       std::move(devtools_proxy)));
    return;
  }

  // Get a process.
  ServiceWorkerStatusCode status = process_manager->AllocateWorkerProcess(
      params->embedded_worker_id, params->scope, params->script_url,
      can_use_existing_process, process_info.get());
  if (status != SERVICE_WORKER_OK) {
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(std::move(callback), status, std::move(params),
                       std::move(process_info), std::move(devtools_proxy)));
    return;
  }
  const int process_id = process_info->process_id;
  RenderProcessHost* rph = RenderProcessHost::FromID(process_id);
  // TODO(falken): This CHECK should no longer fail, so turn to a DCHECK it if
  // crash reports agree. Consider also checking for rph->HasConnection().
  CHECK(rph);

  // Bind |request|, which is attached to |EmbeddedWorkerInstance::client_|, to
  // the process. If the process dies, |client_|'s connection error callback
  // will be called on the IO thread.
  if (request.is_pending())
    BindInterface(rph, std::move(request));

  // Register to DevTools and update params accordingly.
  const int routing_id = rph->GetNextRoutingID();
  params->wait_for_debugger =
      ServiceWorkerDevToolsManager::GetInstance()->WorkerCreated(
          process_id, routing_id,
          ServiceWorkerDevToolsManager::ServiceWorkerIdentifier(
              context, weak_context, params->service_worker_version_id,
              params->script_url, params->scope),
          params->is_installed);
  params->worker_devtools_agent_route_id = routing_id;
  // Create DevToolsProxy here to ensure that the WorkerCreated() call is
  // balanced by DevToolsProxy's destructor calling WorkerDestroyed().
  devtools_proxy = base::MakeUnique<EmbeddedWorkerInstance::DevToolsProxy>(
      process_id, routing_id);

  // Set EmbeddedWorkerSettings for content settings only readable from the UI
  // thread.
  // TODO(bengr): Support changes to the data saver setting while the worker is
  // running.
  params->settings.data_saver_enabled =
      GetContentClient()->browser()->IsDataSaverEnabled(
          process_manager->browser_context());

  // Continue on the IO thread.
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(std::move(callback), status, std::move(params),
                     std::move(process_info), std::move(devtools_proxy)));
}

void CallDetach(EmbeddedWorkerInstance* instance) {
  // This could be called on the UI thread if |client_| still be valid when the
  // message loop on the UI thread gets destructed.
  // TODO(shimazu): Remove this after https://crbug.com/604762 is fixed
  if (!BrowserThread::CurrentlyOn(BrowserThread::IO))
    return;
  instance->Detach();
}

bool HasSentStartWorker(EmbeddedWorkerInstance::StartingPhase phase) {
  switch (phase) {
    case EmbeddedWorkerInstance::NOT_STARTING:
    case EmbeddedWorkerInstance::ALLOCATING_PROCESS:
      return false;
    case EmbeddedWorkerInstance::SENT_START_WORKER:
    case EmbeddedWorkerInstance::SCRIPT_DOWNLOADING:
    case EmbeddedWorkerInstance::SCRIPT_READ_STARTED:
    case EmbeddedWorkerInstance::SCRIPT_READ_FINISHED:
    case EmbeddedWorkerInstance::SCRIPT_STREAMING:
    case EmbeddedWorkerInstance::SCRIPT_LOADED:
    case EmbeddedWorkerInstance::SCRIPT_EVALUATED:
    case EmbeddedWorkerInstance::THREAD_STARTED:
      return true;
    case EmbeddedWorkerInstance::STARTING_PHASE_MAX_VALUE:
      NOTREACHED();
  }
  return false;
}

}  // namespace

// Created on UI thread and moved to IO thread. Proxies notifications to
// DevToolsManager that lives on UI thread. Owned by EmbeddedWorkerInstance.
class EmbeddedWorkerInstance::DevToolsProxy {
 public:
  DevToolsProxy(int process_id, int agent_route_id)
      : process_id_(process_id),
        agent_route_id_(agent_route_id) {}

  ~DevToolsProxy() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(NotifyWorkerDestroyedOnUI,
                   process_id_, agent_route_id_));
  }

  void NotifyWorkerReadyForInspection() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(NotifyWorkerReadyForInspectionOnUI,
                                       process_id_, agent_route_id_));
  }

  void NotifyWorkerVersionInstalled() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(NotifyWorkerVersionInstalledOnUI,
                                       process_id_, agent_route_id_));
  }

  void NotifyWorkerVersionDoomed() {
    DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(NotifyWorkerVersionDoomedOnUI,
                                       process_id_, agent_route_id_));
  }

  bool ShouldNotifyWorkerStopIgnored() const {
    return !worker_stop_ignored_notified_;
  }

  void WorkerStopIgnoredNotified() { worker_stop_ignored_notified_ = true; }

  int agent_route_id() const { return agent_route_id_; }

 private:
  const int process_id_;
  const int agent_route_id_;
  bool worker_stop_ignored_notified_ = false;

  DISALLOW_COPY_AND_ASSIGN(DevToolsProxy);
};

// A handle for a renderer process managed by ServiceWorkerProcessManager on the
// UI thread. Lives on the IO thread.
class EmbeddedWorkerInstance::WorkerProcessHandle {
 public:
  WorkerProcessHandle(const base::WeakPtr<ServiceWorkerContextCore>& context,
                      int embedded_worker_id,
                      int process_id)
      : context_(context),
        embedded_worker_id_(embedded_worker_id),
        process_id_(process_id) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DCHECK_NE(ChildProcessHost::kInvalidUniqueID, process_id_);
  }

  ~WorkerProcessHandle() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    if (!context_)
      return;
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(&ServiceWorkerProcessManager::ReleaseWorkerProcess,
                       context_->process_manager()->AsWeakPtr(),
                       embedded_worker_id_));
  }

  int process_id() const { return process_id_; }

 private:
  base::WeakPtr<ServiceWorkerContextCore> context_;

  const int embedded_worker_id_;
  const int process_id_;

  DISALLOW_COPY_AND_ASSIGN(WorkerProcessHandle);
};

// A task to allocate a worker process and to send a start worker message. This
// is created on EmbeddedWorkerInstance::Start(), owned by the instance and
// destroyed on EmbeddedWorkerInstance::OnScriptEvaluated().
// We can abort starting worker by destroying this task anytime during the
// sequence.
// Lives on the IO thread.
class EmbeddedWorkerInstance::StartTask {
 public:
  enum class ProcessAllocationState { NOT_ALLOCATED, ALLOCATING, ALLOCATED };

  StartTask(EmbeddedWorkerInstance* instance,
            const GURL& script_url,
            mojom::EmbeddedWorkerInstanceClientRequest request)
      : instance_(instance),
        request_(std::move(request)),
        state_(ProcessAllocationState::NOT_ALLOCATED),
        is_installed_(false),
        started_during_browser_startup_(false),
        weak_factory_(this) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN1("ServiceWorker",
                                      "EmbeddedWorkerInstance::Start",
                                      instance_, "Script", script_url.spec());
  }

  ~StartTask() {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    TRACE_EVENT_NESTABLE_ASYNC_END0("ServiceWorker",
                                    "EmbeddedWorkerInstance::Start", instance_);

    if (!instance_->context_)
      return;

    switch (state_) {
      case ProcessAllocationState::NOT_ALLOCATED:
        // Not necessary to release a process.
        break;
      case ProcessAllocationState::ALLOCATING:
        // Abort half-baked process allocation on the UI thread.
        BrowserThread::PostTask(
            BrowserThread::UI, FROM_HERE,
            base::BindOnce(&ServiceWorkerProcessManager::ReleaseWorkerProcess,
                           instance_->context_->process_manager()->AsWeakPtr(),
                           instance_->embedded_worker_id()));
        break;
      case ProcessAllocationState::ALLOCATED:
        // Otherwise, the process will be released by EmbeddedWorkerInstance.
        break;
    }

    // Don't have to abort |start_callback_| here. The caller of
    // EmbeddedWorkerInstance::Start(), that is, ServiceWorkerVersion does not
    // expect it when the start worker sequence is canceled by Stop() because
    // the callback, ServiceWorkerVersion::OnStartSentAndScriptEvaluated(),
    // could drain valid start requests queued in the version. After the worker
    // is stopped, the version attempts to restart the worker if there are
    // requests in the queue. See ServiceWorkerVersion::OnStoppedInternal() for
    // details.
    // TODO(nhiroki): Reconsider this bizarre layering.
  }

  void set_start_worker_sent_time(base::TimeTicks time) {
    start_worker_sent_time_ = time;
  }
  base::TimeTicks start_worker_sent_time() const {
    return start_worker_sent_time_;
  }

  void Start(std::unique_ptr<EmbeddedWorkerStartParams> params,
             const StatusCallback& callback) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    DCHECK(instance_->context_);
    state_ = ProcessAllocationState::ALLOCATING;
    start_callback_ = callback;
    is_installed_ = params->is_installed;

    if (!GetContentClient()->browser()->IsBrowserStartupComplete())
      started_during_browser_startup_ = true;

    bool can_use_existing_process =
        instance_->context_->GetVersionFailureCount(
            params->service_worker_version_id) < kMaxSameProcessFailureCount;
    DCHECK_EQ(params->embedded_worker_id, instance_->embedded_worker_id_);
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("ServiceWorker", "ALLOCATING_PROCESS",
                                      instance_);
    // Hop to the UI thread for process allocation and setup. We will continue
    // on the IO thread in StartTask::OnSetupCompleted().
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::BindOnce(&SetupOnUIThread,
                       instance_->context_->process_manager()->AsWeakPtr(),
                       can_use_existing_process, std::move(params),
                       std::move(request_), instance_->context_.get(),
                       instance_->context_,
                       base::BindOnce(&StartTask::OnSetupCompleted,
                                      weak_factory_.GetWeakPtr())));
  }

  static void RunStartCallback(StartTask* task,
                               ServiceWorkerStatusCode status) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    StatusCallback callback = task->start_callback_;
    task->start_callback_.Reset();
    callback.Run(status);
    // |task| may be destroyed.
  }

  bool is_installed() const { return is_installed_; }

 private:
  void OnSetupCompleted(
      ServiceWorkerStatusCode status,
      std::unique_ptr<EmbeddedWorkerStartParams> params,
      std::unique_ptr<ServiceWorkerProcessManager::AllocatedProcessInfo>
          process_info,
      std::unique_ptr<EmbeddedWorkerInstance::DevToolsProxy> devtools_proxy) {
    DCHECK_CURRENTLY_ON(BrowserThread::IO);
    if (status != SERVICE_WORKER_OK) {
      TRACE_EVENT_NESTABLE_ASYNC_END1("ServiceWorker", "ALLOCATING_PROCESS",
                                      instance_, "Error",
                                      ServiceWorkerStatusToString(status));
      StatusCallback callback = start_callback_;
      start_callback_.Reset();
      instance_->OnStartFailed(callback, status);
      // |this| may be destroyed.
      return;
    }

    ServiceWorkerMetrics::StartSituation start_situation =
        process_info->start_situation;
    TRACE_EVENT_NESTABLE_ASYNC_END1(
        "ServiceWorker", "ALLOCATING_PROCESS", instance_, "StartSituation",
        ServiceWorkerMetrics::StartSituationToString(start_situation));
    if (is_installed_) {
      ServiceWorkerMetrics::RecordProcessCreated(
          start_situation == ServiceWorkerMetrics::StartSituation::NEW_PROCESS);
    }

    if (started_during_browser_startup_)
      start_situation = ServiceWorkerMetrics::StartSituation::DURING_STARTUP;

    // Notify the instance that a process is allocated.
    state_ = ProcessAllocationState::ALLOCATED;
    instance_->OnProcessAllocated(
        base::MakeUnique<WorkerProcessHandle>(instance_->context_,
                                              instance_->embedded_worker_id(),
                                              process_info->process_id),
        start_situation);

    // Notify the instance that it is registered to the DevTools manager.
    instance_->OnRegisteredToDevToolsManager(std::move(devtools_proxy),
                                             params->wait_for_debugger);

    status = instance_->SendStartWorker(std::move(params));
    if (status != SERVICE_WORKER_OK) {
      StatusCallback callback = start_callback_;
      start_callback_.Reset();
      instance_->OnStartFailed(callback, status);
      // |this| may be destroyed.
    }

    // |this|'s work is done here, but |instance_| still uses its state until
    // startup is complete.
  }

  // |instance_| must outlive |this|.
  EmbeddedWorkerInstance* instance_;

  // Ownership is transferred by base::Passed() to a task after process
  // allocation.
  mojom::EmbeddedWorkerInstanceClientRequest request_;

  StatusCallback start_callback_;
  ProcessAllocationState state_;

  // Used for UMA.
  bool is_installed_;
  bool started_during_browser_startup_;
  base::TimeTicks start_worker_sent_time_;

  base::WeakPtrFactory<StartTask> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(StartTask);
};

bool EmbeddedWorkerInstance::Listener::OnMessageReceived(
    const IPC::Message& message) {
  return false;
}

EmbeddedWorkerInstance::~EmbeddedWorkerInstance() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(status_ == EmbeddedWorkerStatus::STOPPING ||
         status_ == EmbeddedWorkerStatus::STOPPED)
      << static_cast<int>(status_);
  devtools_proxy_.reset();
  if (registry_->GetWorker(embedded_worker_id_))
    registry_->RemoveWorker(process_id(), embedded_worker_id_);
  process_handle_.reset();
}

void EmbeddedWorkerInstance::Start(
    std::unique_ptr<EmbeddedWorkerStartParams> params,
    mojom::ServiceWorkerEventDispatcherRequest dispatcher_request,
    mojom::ServiceWorkerInstalledScriptsInfoPtr installed_scripts_info,
    const StatusCallback& callback) {
  restart_count_++;
  if (!context_) {
    callback.Run(SERVICE_WORKER_ERROR_ABORT);
    // |this| may be destroyed by the callback.
    return;
  }
  DCHECK(status_ == EmbeddedWorkerStatus::STOPPED);

  DCHECK(!params->pause_after_download || !params->is_installed);
  DCHECK_NE(kInvalidServiceWorkerVersionId, params->service_worker_version_id);

  step_time_ = base::TimeTicks::Now();
  status_ = EmbeddedWorkerStatus::STARTING;
  starting_phase_ = ALLOCATING_PROCESS;
  network_accessed_for_script_ = false;
  for (auto& observer : listener_list_)
    observer.OnStarting();

  params->embedded_worker_id = embedded_worker_id_;
  params->worker_devtools_agent_route_id = MSG_ROUTING_NONE;
  params->wait_for_debugger = false;
  params->settings.v8_cache_options = GetV8CacheOptions();

  mojom::EmbeddedWorkerInstanceClientRequest request =
      mojo::MakeRequest(&client_);
  client_.set_connection_error_handler(
      base::Bind(&CallDetach, base::Unretained(this)));

  pending_dispatcher_request_ = std::move(dispatcher_request);
  pending_installed_scripts_info_ = std::move(installed_scripts_info);

  inflight_start_task_.reset(
      new StartTask(this, params->script_url, std::move(request)));
  inflight_start_task_->Start(std::move(params), callback);
}

bool EmbeddedWorkerInstance::Stop() {
  DCHECK(status_ == EmbeddedWorkerStatus::STARTING ||
         status_ == EmbeddedWorkerStatus::RUNNING)
      << static_cast<int>(status_);

  // Abort an inflight start task.
  inflight_start_task_.reset();

  if (status_ == EmbeddedWorkerStatus::STARTING &&
      !HasSentStartWorker(starting_phase())) {
    // Don't send the StopWorker message when the StartWorker message hasn't
    // been sent.
    // TODO(shimazu): Invoke OnStopping/OnStopped after the legacy IPC path is
    // removed.
    OnDetached();
    return false;
  }
  client_->StopWorker();

  status_ = EmbeddedWorkerStatus::STOPPING;
  for (auto& observer : listener_list_)
    observer.OnStopping();
  return true;
}

void EmbeddedWorkerInstance::StopIfIdle() {
  if (devtools_attached_) {
    if (devtools_proxy_) {
      // Check ShouldNotifyWorkerStopIgnored not to show the same message
      // multiple times in DevTools.
      if (devtools_proxy_->ShouldNotifyWorkerStopIgnored()) {
        AddMessageToConsole(blink::WebConsoleMessage::kLevelVerbose,
                            kServiceWorkerTerminationCanceledMesage);
        devtools_proxy_->WorkerStopIgnoredNotified();
      }
    }
    return;
  }
  Stop();
}

ServiceWorkerStatusCode EmbeddedWorkerInstance::SendMessage(
    const IPC::Message& message) {
  DCHECK_NE(kInvalidEmbeddedWorkerThreadId, thread_id_);
  if (status_ != EmbeddedWorkerStatus::RUNNING &&
      status_ != EmbeddedWorkerStatus::STARTING) {
    return SERVICE_WORKER_ERROR_IPC_FAILED;
  }
  return registry_->Send(process_id(),
                         new EmbeddedWorkerContextMsg_MessageToWorker(
                             thread_id_, embedded_worker_id_, message));
}

void EmbeddedWorkerInstance::ResumeAfterDownload() {
  if (process_id() == ChildProcessHost::kInvalidUniqueID ||
      status_ != EmbeddedWorkerStatus::STARTING) {
    return;
  }
  DCHECK(client_.is_bound());
  client_->ResumeAfterDownload();
}

EmbeddedWorkerInstance::EmbeddedWorkerInstance(
    base::WeakPtr<ServiceWorkerContextCore> context,
    int embedded_worker_id)
    : context_(context),
      registry_(context->embedded_worker_registry()),
      embedded_worker_id_(embedded_worker_id),
      status_(EmbeddedWorkerStatus::STOPPED),
      starting_phase_(NOT_STARTING),
      restart_count_(0),
      thread_id_(kInvalidEmbeddedWorkerThreadId),
      instance_host_binding_(this),
      devtools_attached_(false),
      network_accessed_for_script_(false),
      weak_factory_(this) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
}

void EmbeddedWorkerInstance::OnProcessAllocated(
    std::unique_ptr<WorkerProcessHandle> handle,
    ServiceWorkerMetrics::StartSituation start_situation) {
  DCHECK_EQ(EmbeddedWorkerStatus::STARTING, status_);
  DCHECK(!process_handle_);

  process_handle_ = std::move(handle);
  start_situation_ = start_situation;
  for (auto& observer : listener_list_)
    observer.OnProcessAllocated();
}

void EmbeddedWorkerInstance::OnRegisteredToDevToolsManager(
    std::unique_ptr<DevToolsProxy> devtools_proxy,
    bool wait_for_debugger) {
  if (devtools_proxy) {
    DCHECK(!devtools_proxy_);
    devtools_proxy_ = std::move(devtools_proxy);
  }
  if (wait_for_debugger) {
    // We don't measure the start time when wait_for_debugger flag is set. So
    // we set the NULL time here.
    step_time_ = base::TimeTicks();
  }
  for (auto& observer : listener_list_)
    observer.OnRegisteredToDevToolsManager();
}

ServiceWorkerStatusCode EmbeddedWorkerInstance::SendStartWorker(
    std::unique_ptr<EmbeddedWorkerStartParams> params) {
  if (!context_)
    return SERVICE_WORKER_ERROR_ABORT;
  if (!context_->GetDispatcherHost(process_id())) {
    // Check if there's a dispatcher host, which is a good sign the process is
    // still alive. It's possible that previously the process crashed, and the
    // Mojo connection error via |client_| detected it and this instance was
    // detached, but on restart ServiceWorkerProcessManager assigned us the
    // process again before RenderProcessHostImpl itself or
    // ServiceWorkerProcessManager knew it crashed, and by the time we get here
    // RenderProcessHostImpl::EnableSendQueue may have been called in
    // anticipation of the RPHI being reused for another renderer process, so
    // Mojo doesn't consider it an error. See https://crbug.com/732729.
    return SERVICE_WORKER_ERROR_IPC_FAILED;
  }
  DCHECK(pending_dispatcher_request_.is_pending());

  DCHECK(!instance_host_binding_.is_bound());
  mojom::EmbeddedWorkerInstanceHostAssociatedPtrInfo host_ptr_info;
  instance_host_binding_.Bind(mojo::MakeRequest(&host_ptr_info));

  const bool is_script_streaming = !pending_installed_scripts_info_.is_null();
  inflight_start_task_->set_start_worker_sent_time(base::TimeTicks::Now());
  client_->StartWorker(*params, std::move(pending_dispatcher_request_),
                       std::move(pending_installed_scripts_info_),
                       std::move(host_ptr_info));
  registry_->BindWorkerToProcess(process_id(), embedded_worker_id());
  OnStartWorkerMessageSent(is_script_streaming);
  if (starting_phase() == SCRIPT_STREAMING) {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("ServiceWorker",
                                      "SENT_START_WITH_SCRIPT_STREAMING", this);
  } else {
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("ServiceWorker", "SENT_START_WORKER",
                                      this);
  }
  return SERVICE_WORKER_OK;
}

void EmbeddedWorkerInstance::OnStartWorkerMessageSent(
    bool is_script_streaming) {
  if (!step_time_.is_null()) {
    base::TimeDelta duration = UpdateStepTime();
    if (inflight_start_task_->is_installed()) {
      ServiceWorkerMetrics::RecordTimeToSendStartWorker(duration,
                                                        start_situation_);
    }
  }

  starting_phase_ = is_script_streaming ? SCRIPT_STREAMING : SENT_START_WORKER;
  for (auto& observer : listener_list_)
    observer.OnStartWorkerMessageSent();
}

void EmbeddedWorkerInstance::OnReadyForInspection() {
  if (devtools_proxy_)
    devtools_proxy_->NotifyWorkerReadyForInspection();
}

void EmbeddedWorkerInstance::OnScriptReadStarted() {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("ServiceWorker", "SCRIPT_READING_CACHE",
                                    this);
  starting_phase_ = SCRIPT_READ_STARTED;
}

void EmbeddedWorkerInstance::OnScriptReadFinished() {
  TRACE_EVENT_NESTABLE_ASYNC_END0("ServiceWorker", "SCRIPT_READING_CACHE",
                                  this);
  starting_phase_ = SCRIPT_READ_FINISHED;
}

void EmbeddedWorkerInstance::OnScriptLoaded() {
  using LoadSource = ServiceWorkerMetrics::LoadSource;

  if (!inflight_start_task_)
    return;
  LoadSource source;
  if (network_accessed_for_script_) {
    DCHECK(!inflight_start_task_->is_installed());
    source = LoadSource::NETWORK;
  } else if (inflight_start_task_->is_installed()) {
    source = LoadSource::SERVICE_WORKER_STORAGE;
  } else {
    source = LoadSource::HTTP_CACHE;
  }

  switch (starting_phase_) {
    case SCRIPT_DOWNLOADING:
      TRACE_EVENT_NESTABLE_ASYNC_END0("ServiceWorker", "SCRIPT_DOWNLOADING",
                                      this);
      break;
    case SCRIPT_STREAMING:
      TRACE_EVENT_NESTABLE_ASYNC_END0("ServiceWorker",
                                      "SENT_START_WITH_SCRIPT_STREAMING", this);
      break;
    default:
      TRACE_EVENT_NESTABLE_ASYNC_END1(
          "ServiceWorker", "SCRIPT_LOADING", this, "Source",
          ServiceWorkerMetrics::LoadSourceToString(source));
      break;
  }

  // Don't record the time when script streaming is enabled because
  // OnScriptLoaded is called at the different timing.
  if (starting_phase_ != SCRIPT_STREAMING && !step_time_.is_null()) {
    base::TimeDelta duration = UpdateStepTime();
    ServiceWorkerMetrics::RecordTimeToLoad(duration, source, start_situation_);
  }

  // Renderer side has started to launch the worker thread.
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("ServiceWorker", "LAUNCHING_WORKER_THREAD",
                                    this);
  starting_phase_ = SCRIPT_LOADED;
  for (auto& observer : listener_list_)
    observer.OnScriptLoaded();
  // |this| may be destroyed by the callback.
}

void EmbeddedWorkerInstance::OnURLJobCreatedForMainScript() {
  TRACE_EVENT_NESTABLE_ASYNC_END0("ServiceWorker", "SENT_START_WORKER", this);
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("ServiceWorker", "SCRIPT_LOADING", this);
  if (!inflight_start_task_)
    return;

  if (!step_time_.is_null()) {
    base::TimeDelta duration = UpdateStepTime();
    if (inflight_start_task_->is_installed())
      ServiceWorkerMetrics::RecordTimeToURLJob(duration, start_situation_);
  }
}

void EmbeddedWorkerInstance::OnWorkerVersionInstalled() {
  if (devtools_proxy_)
    devtools_proxy_->NotifyWorkerVersionInstalled();
}

void EmbeddedWorkerInstance::OnWorkerVersionDoomed() {
  if (devtools_proxy_)
    devtools_proxy_->NotifyWorkerVersionDoomed();
}

void EmbeddedWorkerInstance::OnThreadStarted(int thread_id, int provider_id) {
  if (!context_ || !inflight_start_task_)
    return;

  ServiceWorkerProviderHost* provider_host =
      context_->GetProviderHost(process_id(), provider_id);
  if (!provider_host) {
    bad_message::ReceivedBadMessage(
        process_id(), bad_message::SWDH_WORKER_SCRIPT_LOAD_NO_HOST);
    return;
  }

  provider_host->SetReadyToSendMessagesToWorker(thread_id);

  TRACE_EVENT_NESTABLE_ASYNC_END0("ServiceWorker", "LAUNCHING_WORKER_THREAD",
                                  this);
  // Renderer side has started to evaluate the loaded worker script.
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("ServiceWorker", "EVALUATING_SCRIPT", this);
  starting_phase_ = THREAD_STARTED;
  if (!step_time_.is_null()) {
    base::TimeDelta duration = UpdateStepTime();
    if (inflight_start_task_->is_installed())
      ServiceWorkerMetrics::RecordTimeToStartThread(duration, start_situation_);
  }

  thread_id_ = thread_id;
  for (auto& observer : listener_list_)
    observer.OnThreadStarted();
}

void EmbeddedWorkerInstance::OnScriptLoadFailed() {
  if (!inflight_start_task_)
    return;

  // starting_phase_ may be SCRIPT_READ_FINISHED in case of reading from cache.
  if (starting_phase_ == SCRIPT_DOWNLOADING) {
    TRACE_EVENT_NESTABLE_ASYNC_END0("ServiceWorker", "SCRIPT_DOWNLOADING",
                                    this);
  }
  TRACE_EVENT_NESTABLE_ASYNC_END1("ServiceWorker", "SCRIPT_LOADING", this,
                                  "Error", "Script load failed.");

  for (auto& observer : listener_list_)
    observer.OnScriptLoadFailed();
}

void EmbeddedWorkerInstance::OnScriptEvaluated(bool success) {
  if (!inflight_start_task_)
    return;

  DCHECK_EQ(EmbeddedWorkerStatus::STARTING, status_);

  // Renderer side has completed evaluating the loaded worker script.
  TRACE_EVENT_NESTABLE_ASYNC_END1("ServiceWorker", "EVALUATING_SCRIPT", this,
                                  "Success", success);
  starting_phase_ = SCRIPT_EVALUATED;
  if (!step_time_.is_null()) {
    base::TimeDelta duration = UpdateStepTime();
    if (success && inflight_start_task_->is_installed())
      ServiceWorkerMetrics::RecordTimeToEvaluateScript(duration,
                                                       start_situation_);
  }

  if (success) {
    // Renderer side has started the final preparations to complete the start
    // process.
    TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("ServiceWorker",
                                      "WAITING_FOR_START_COMPLETE", this);
  }
  base::WeakPtr<EmbeddedWorkerInstance> weak_this = weak_factory_.GetWeakPtr();
  StartTask::RunStartCallback(
      inflight_start_task_.get(),
      success ? SERVICE_WORKER_OK
              : SERVICE_WORKER_ERROR_SCRIPT_EVALUATE_FAILED);
  // |this| may be destroyed by the callback.
}

void EmbeddedWorkerInstance::OnStarted(
    mojom::EmbeddedWorkerStartTimingPtr start_timing) {
  if (!registry_->OnWorkerStarted(process_id(), embedded_worker_id_))
    return;
  // Stop is requested before OnStarted is sent back from the worker.
  if (status_ == EmbeddedWorkerStatus::STOPPING)
    return;

  if (inflight_start_task_->is_installed()) {
    ServiceWorkerMetrics::RecordEmbeddedWorkerStartTiming(
        std::move(start_timing), inflight_start_task_->start_worker_sent_time(),
        start_situation_);
  }

  TRACE_EVENT_NESTABLE_ASYNC_END0("ServiceWorker", "WAITING_FOR_START_COMPLETE",
                                  this);
  DCHECK(status_ == EmbeddedWorkerStatus::STARTING);
  status_ = EmbeddedWorkerStatus::RUNNING;
  inflight_start_task_.reset();
  for (auto& observer : listener_list_)
    observer.OnStarted();
}

void EmbeddedWorkerInstance::OnStopped() {
  registry_->OnWorkerStopped(process_id(), embedded_worker_id_);

  EmbeddedWorkerStatus old_status = status_;
  ReleaseProcess();
  for (auto& observer : listener_list_)
    observer.OnStopped(old_status);
}

void EmbeddedWorkerInstance::OnDetached() {
  EmbeddedWorkerStatus old_status = status_;
  ReleaseProcess();
  for (auto& observer : listener_list_)
    observer.OnDetached(old_status);
}

void EmbeddedWorkerInstance::Detach() {
  registry_->DetachWorker(process_id(), embedded_worker_id());
  OnDetached();
}

base::WeakPtr<EmbeddedWorkerInstance> EmbeddedWorkerInstance::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

bool EmbeddedWorkerInstance::OnMessageReceived(const IPC::Message& message) {
  for (auto& listener : listener_list_) {
    if (listener.OnMessageReceived(message))
      return true;
  }
  return false;
}

void EmbeddedWorkerInstance::OnReportException(
    const base::string16& error_message,
    int line_number,
    int column_number,
    const GURL& source_url) {
  for (auto& observer : listener_list_) {
    observer.OnReportException(error_message, line_number, column_number,
                               source_url);
  }
}

void EmbeddedWorkerInstance::OnReportConsoleMessage(
    int source_identifier,
    int message_level,
    const base::string16& message,
    int line_number,
    const GURL& source_url) {
  for (auto& observer : listener_list_) {
    observer.OnReportConsoleMessage(source_identifier, message_level, message,
                                    line_number, source_url);
  }
}

int EmbeddedWorkerInstance::process_id() const {
  if (process_handle_)
    return process_handle_->process_id();
  return ChildProcessHost::kInvalidUniqueID;
}

int EmbeddedWorkerInstance::worker_devtools_agent_route_id() const {
  if (devtools_proxy_)
    return devtools_proxy_->agent_route_id();
  return MSG_ROUTING_NONE;
}

void EmbeddedWorkerInstance::AddListener(Listener* listener) {
  listener_list_.AddObserver(listener);
}

void EmbeddedWorkerInstance::RemoveListener(Listener* listener) {
  listener_list_.RemoveObserver(listener);
}

void EmbeddedWorkerInstance::SetDevToolsAttached(bool attached) {
  devtools_attached_ = attached;
  if (attached)
    registry_->OnDevToolsAttached(embedded_worker_id_);
}

void EmbeddedWorkerInstance::OnNetworkAccessedForScriptLoad() {
  TRACE_EVENT_NESTABLE_ASYNC_BEGIN0("ServiceWorker", "SCRIPT_DOWNLOADING",
                                    this);
  starting_phase_ = SCRIPT_DOWNLOADING;
  network_accessed_for_script_ = true;
}

void EmbeddedWorkerInstance::ReleaseProcess() {
  // Abort an inflight start task.
  inflight_start_task_.reset();

  client_.reset();
  instance_host_binding_.Close();
  devtools_proxy_.reset();
  process_handle_.reset();
  status_ = EmbeddedWorkerStatus::STOPPED;
  starting_phase_ = NOT_STARTING;
  thread_id_ = kInvalidEmbeddedWorkerThreadId;
}

void EmbeddedWorkerInstance::OnStartFailed(const StatusCallback& callback,
                                           ServiceWorkerStatusCode status) {
  EmbeddedWorkerStatus old_status = status_;
  ReleaseProcess();
  base::WeakPtr<EmbeddedWorkerInstance> weak_this = weak_factory_.GetWeakPtr();
  callback.Run(status);
  if (weak_this && old_status != EmbeddedWorkerStatus::STOPPED) {
    for (auto& observer : weak_this->listener_list_)
      observer.OnStopped(old_status);
  }
}

base::TimeDelta EmbeddedWorkerInstance::UpdateStepTime() {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(!step_time_.is_null());
  base::TimeTicks now = base::TimeTicks::Now();
  base::TimeDelta duration = now - step_time_;
  step_time_ = now;
  return duration;
}

void EmbeddedWorkerInstance::AddMessageToConsole(
    blink::WebConsoleMessage::Level level,
    const std::string& message) {
  if (process_id() == ChildProcessHost::kInvalidUniqueID)
    return;
  DCHECK(client_.is_bound());
  client_->AddMessageToConsole(level, message);
}

// static
std::string EmbeddedWorkerInstance::StatusToString(
    EmbeddedWorkerStatus status) {
  switch (status) {
    case EmbeddedWorkerStatus::STOPPED:
      return "STOPPED";
    case EmbeddedWorkerStatus::STARTING:
      return "STARTING";
    case EmbeddedWorkerStatus::RUNNING:
      return "RUNNING";
    case EmbeddedWorkerStatus::STOPPING:
      return "STOPPING";
  }
  NOTREACHED() << static_cast<int>(status);
  return std::string();
}

// static
std::string EmbeddedWorkerInstance::StartingPhaseToString(StartingPhase phase) {
  switch (phase) {
    case NOT_STARTING:
      return "Not in STARTING status";
    case ALLOCATING_PROCESS:
      return "Allocating process";
    case SENT_START_WORKER:
      return "Sent StartWorker message to renderer";
    case SCRIPT_DOWNLOADING:
      return "Script downloading";
    case SCRIPT_LOADED:
      return "Script loaded";
    case SCRIPT_EVALUATED:
      return "Script evaluated";
    case THREAD_STARTED:
      return "Thread started";
    case SCRIPT_READ_STARTED:
      return "Script read started";
    case SCRIPT_READ_FINISHED:
      return "Script read finished";
    case SCRIPT_STREAMING:
      return "Script streaming";
    case STARTING_PHASE_MAX_VALUE:
      NOTREACHED();
  }
  NOTREACHED() << phase;
  return std::string();
}

}  // namespace content
