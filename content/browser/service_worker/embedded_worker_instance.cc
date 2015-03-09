// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/embedded_worker_instance.h"

#include <algorithm>
#include <utility>

#include "base/bind_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/non_thread_safe.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/devtools/service_worker_devtools_manager.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/common/content_switches_internal.h"
#include "content/common/service_worker/embedded_worker_messages.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "ipc/ipc_message.h"
#include "url/gurl.h"

namespace content {

namespace {

// Functor to sort by the .second element of a struct.
struct SecondGreater {
  template <typename Value>
  bool operator()(const Value& lhs, const Value& rhs) {
    return lhs.second > rhs.second;
  }
};

void NotifyWorkerReadyForInspectionOnUI(int worker_process_id,
                                        int worker_route_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ServiceWorkerDevToolsManager::GetInstance()->WorkerReadyForInspection(
      worker_process_id, worker_route_id);
}

void NotifyWorkerDestroyedOnUI(int worker_process_id, int worker_route_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ServiceWorkerDevToolsManager::GetInstance()->WorkerDestroyed(
      worker_process_id, worker_route_id);
}

void NotifyWorkerStopIgnoredOnUI(int worker_process_id, int worker_route_id) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  ServiceWorkerDevToolsManager::GetInstance()->WorkerStopIgnored(
      worker_process_id, worker_route_id);
}

void RegisterToWorkerDevToolsManagerOnUI(
    int process_id,
    const ServiceWorkerContextCore* service_worker_context,
    const base::WeakPtr<ServiceWorkerContextCore>& service_worker_context_weak,
    int64 service_worker_version_id,
    const GURL& url,
    const base::Callback<void(int worker_devtools_agent_route_id,
                              bool wait_for_debugger)>& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  int worker_devtools_agent_route_id = MSG_ROUTING_NONE;
  bool wait_for_debugger = false;
  if (RenderProcessHost* rph = RenderProcessHost::FromID(process_id)) {
    // |rph| may be NULL in unit tests.
    worker_devtools_agent_route_id = rph->GetNextRoutingID();
    wait_for_debugger =
        ServiceWorkerDevToolsManager::GetInstance()->WorkerCreated(
            process_id,
            worker_devtools_agent_route_id,
            ServiceWorkerDevToolsManager::ServiceWorkerIdentifier(
                service_worker_context,
                service_worker_context_weak,
                service_worker_version_id,
                url));
  }
  BrowserThread::PostTask(
      BrowserThread::IO,
      FROM_HERE,
      base::Bind(callback, worker_devtools_agent_route_id, wait_for_debugger));
}

}  // namespace

// Lives on IO thread, proxies notifications to DevToolsManager that lives on
// UI thread. Owned by EmbeddedWorkerInstance.
class EmbeddedWorkerInstance::DevToolsProxy : public base::NonThreadSafe {
 public:
  DevToolsProxy(int process_id, int agent_route_id)
      : process_id_(process_id),
        agent_route_id_(agent_route_id) {}

  ~DevToolsProxy() {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(NotifyWorkerDestroyedOnUI,
                   process_id_, agent_route_id_));
  }

  void NotifyWorkerReadyForInspection() {
    DCHECK(CalledOnValidThread());
    BrowserThread::PostTask(BrowserThread::UI, FROM_HERE,
                            base::Bind(NotifyWorkerReadyForInspectionOnUI,
                                       process_id_, agent_route_id_));
  }

  void NotifyWorkerStopIgnored() {
    DCHECK(CalledOnValidThread());
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            base::Bind(NotifyWorkerStopIgnoredOnUI,
                                       process_id_, agent_route_id_));
  }

  int agent_route_id() const { return agent_route_id_; }

 private:
  const int process_id_;
  const int agent_route_id_;
  DISALLOW_COPY_AND_ASSIGN(DevToolsProxy);
};

EmbeddedWorkerInstance::~EmbeddedWorkerInstance() {
  if (status_ == STARTING || status_ == RUNNING)
    Stop();
  devtools_proxy_.reset();
  if (context_ && process_id_ != -1)
    context_->process_manager()->ReleaseWorkerProcess(embedded_worker_id_);
  registry_->RemoveWorker(process_id_, embedded_worker_id_);
}

void EmbeddedWorkerInstance::Start(int64 service_worker_version_id,
                                   const GURL& scope,
                                   const GURL& script_url,
                                   bool pause_after_download,
                                   const StatusCallback& callback) {
  if (!context_) {
    callback.Run(SERVICE_WORKER_ERROR_ABORT);
    return;
  }
  DCHECK(status_ == STOPPED);
  start_timing_ = base::TimeTicks::Now();
  status_ = STARTING;
  starting_phase_ = ALLOCATING_PROCESS;
  network_accessed_for_script_ = false;
  scoped_ptr<EmbeddedWorkerMsg_StartWorker_Params> params(
      new EmbeddedWorkerMsg_StartWorker_Params());
  TRACE_EVENT_ASYNC_BEGIN2("ServiceWorker",
                           "EmbeddedWorkerInstance::ProcessAllocate",
                           params.get(),
                           "Scope", scope.spec(),
                           "Script URL", script_url.spec());
  params->embedded_worker_id = embedded_worker_id_;
  params->service_worker_version_id = service_worker_version_id;
  params->scope = scope;
  params->script_url = script_url;
  params->worker_devtools_agent_route_id = MSG_ROUTING_NONE;
  params->pause_after_download = pause_after_download;
  params->wait_for_debugger = false;
  params->v8_cache_options = GetV8CacheOptions();
  context_->process_manager()->AllocateWorkerProcess(
      embedded_worker_id_,
      scope,
      script_url,
      base::Bind(&EmbeddedWorkerInstance::RunProcessAllocated,
                 weak_factory_.GetWeakPtr(),
                 context_,
                 base::Passed(&params),
                 callback));
}

ServiceWorkerStatusCode EmbeddedWorkerInstance::Stop() {
  DCHECK(status_ == STARTING || status_ == RUNNING) << status_;
  ServiceWorkerStatusCode status =
      registry_->StopWorker(process_id_, embedded_worker_id_);
  if (status == SERVICE_WORKER_OK)
    status_ = STOPPING;
  return status;
}

void EmbeddedWorkerInstance::StopIfIdle() {
  if (devtools_attached_) {
    if (devtools_proxy_)
      devtools_proxy_->NotifyWorkerStopIgnored();
    return;
  }
  Stop();
}

void EmbeddedWorkerInstance::ResumeAfterDownload() {
  DCHECK_EQ(STARTING, status_);
  registry_->Send(
      process_id_,
      new EmbeddedWorkerMsg_ResumeAfterDownload(embedded_worker_id_));
}

ServiceWorkerStatusCode EmbeddedWorkerInstance::SendMessage(
    const IPC::Message& message) {
  DCHECK_NE(kInvalidEmbeddedWorkerThreadId, thread_id_);
  if (status_ != RUNNING && status_ != STARTING)
    return SERVICE_WORKER_ERROR_IPC_FAILED;
  return registry_->Send(process_id_,
                         new EmbeddedWorkerContextMsg_MessageToWorker(
                             thread_id_, embedded_worker_id_, message));
}

EmbeddedWorkerInstance::EmbeddedWorkerInstance(
    base::WeakPtr<ServiceWorkerContextCore> context,
    int embedded_worker_id)
    : context_(context),
      registry_(context->embedded_worker_registry()),
      embedded_worker_id_(embedded_worker_id),
      status_(STOPPED),
      starting_phase_(NOT_STARTING),
      process_id_(-1),
      thread_id_(kInvalidEmbeddedWorkerThreadId),
      devtools_attached_(false),
      network_accessed_for_script_(false),
      weak_factory_(this) {
}

// static
void EmbeddedWorkerInstance::RunProcessAllocated(
    base::WeakPtr<EmbeddedWorkerInstance> instance,
    base::WeakPtr<ServiceWorkerContextCore> context,
    scoped_ptr<EmbeddedWorkerMsg_StartWorker_Params> params,
    const EmbeddedWorkerInstance::StatusCallback& callback,
    ServiceWorkerStatusCode status,
    int process_id) {
  if (!context) {
    callback.Run(SERVICE_WORKER_ERROR_ABORT);
    return;
  }
  if (!instance) {
    if (status == SERVICE_WORKER_OK) {
      // We only have a process allocated if the status is OK.
      context->process_manager()->ReleaseWorkerProcess(
          params->embedded_worker_id);
    }
    callback.Run(SERVICE_WORKER_ERROR_ABORT);
    return;
  }
  instance->ProcessAllocated(params.Pass(), callback, process_id, status);
}

void EmbeddedWorkerInstance::ProcessAllocated(
    scoped_ptr<EmbeddedWorkerMsg_StartWorker_Params> params,
    const StatusCallback& callback,
    int process_id,
    ServiceWorkerStatusCode status) {
  DCHECK_EQ(process_id_, -1);
  TRACE_EVENT_ASYNC_END1("ServiceWorker",
                         "EmbeddedWorkerInstance::ProcessAllocate",
                         params.get(),
                         "Status", status);
  if (status != SERVICE_WORKER_OK) {
    status_ = STOPPED;
    callback.Run(status);
    return;
  }
  const int64 service_worker_version_id = params->service_worker_version_id;
  process_id_ = process_id;
  GURL script_url(params->script_url);

  // Register this worker to DevToolsManager on UI thread, then continue to
  // call SendStartWorker on IO thread.
  starting_phase_ = REGISTERING_TO_DEVTOOLS;
  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(RegisterToWorkerDevToolsManagerOnUI,
                 process_id_,
                 context_.get(),
                 context_,
                 service_worker_version_id,
                 script_url,
                 base::Bind(&EmbeddedWorkerInstance::SendStartWorker,
                            weak_factory_.GetWeakPtr(),
                            base::Passed(&params),
                            callback)));
}

void EmbeddedWorkerInstance::SendStartWorker(
    scoped_ptr<EmbeddedWorkerMsg_StartWorker_Params> params,
    const StatusCallback& callback,
    int worker_devtools_agent_route_id,
    bool wait_for_debugger) {
  if (worker_devtools_agent_route_id != MSG_ROUTING_NONE) {
    DCHECK(!devtools_proxy_);
    devtools_proxy_.reset(new DevToolsProxy(process_id_,
                                            worker_devtools_agent_route_id));
  }
  params->worker_devtools_agent_route_id = worker_devtools_agent_route_id;
  params->wait_for_debugger = wait_for_debugger;
  if (params->pause_after_download || params->wait_for_debugger) {
    // We don't measure the start time when pause_after_download or
    // wait_for_debugger flag is set. So we set the NULL time here.
    start_timing_ = base::TimeTicks();
  } else {
    DCHECK(!start_timing_.is_null());
    UMA_HISTOGRAM_TIMES("EmbeddedWorkerInstance.ProcessAllocation",
                        base::TimeTicks::Now() - start_timing_);
    // Reset |start_timing_| to measure the time excluding the process
    // allocation time.
    start_timing_ = base::TimeTicks::Now();
  }

  starting_phase_ = SENT_START_WORKER;
  ServiceWorkerStatusCode status =
      registry_->SendStartWorker(params.Pass(), process_id_);
  if (status != SERVICE_WORKER_OK) {
    callback.Run(status);
    return;
  }
  DCHECK(start_callback_.is_null());
  start_callback_ = callback;
}

void EmbeddedWorkerInstance::OnReadyForInspection() {
  if (devtools_proxy_)
    devtools_proxy_->NotifyWorkerReadyForInspection();
}

void EmbeddedWorkerInstance::OnScriptLoaded(int thread_id) {
  starting_phase_ = SCRIPT_LOADED;
  if (!start_timing_.is_null()) {
    if (network_accessed_for_script_) {
      UMA_HISTOGRAM_TIMES("EmbeddedWorkerInstance.ScriptLoadWithNetworkAccess",
                          base::TimeTicks::Now() - start_timing_);
    } else {
      UMA_HISTOGRAM_TIMES(
          "EmbeddedWorkerInstance.ScriptLoadWithoutNetworkAccess",
          base::TimeTicks::Now() - start_timing_);
    }
    // Reset |start_timing_| to measure the time excluding the process
    // allocation time and the script loading time.
    start_timing_ = base::TimeTicks::Now();
  }
  thread_id_ = thread_id;
  FOR_EACH_OBSERVER(Listener, listener_list_, OnScriptLoaded());
}

void EmbeddedWorkerInstance::OnScriptLoadFailed() {
}

void EmbeddedWorkerInstance::OnScriptEvaluated(bool success) {
  starting_phase_ = SCRIPT_EVALUATED;
  if (success && !start_timing_.is_null()) {
    UMA_HISTOGRAM_TIMES("EmbeddedWorkerInstance.ScriptEvaluate",
                        base::TimeTicks::Now() - start_timing_);
  }
  DCHECK(!start_callback_.is_null());
  start_callback_.Run(success ? SERVICE_WORKER_OK
                              : SERVICE_WORKER_ERROR_START_WORKER_FAILED);
  start_callback_.Reset();
}

void EmbeddedWorkerInstance::OnStarted() {
  // Stop is requested before OnStarted is sent back from the worker.
  if (status_ == STOPPING)
    return;
  DCHECK(status_ == STARTING);
  status_ = RUNNING;
  FOR_EACH_OBSERVER(Listener, listener_list_, OnStarted());
}

void EmbeddedWorkerInstance::OnStopped() {
  devtools_proxy_.reset();
  if (context_)
    context_->process_manager()->ReleaseWorkerProcess(embedded_worker_id_);
  Status old_status = status_;
  status_ = STOPPED;
  process_id_ = -1;
  thread_id_ = -1;
  start_callback_.Reset();
  FOR_EACH_OBSERVER(Listener, listener_list_, OnStopped(old_status));
}

void EmbeddedWorkerInstance::OnPausedAfterDownload() {
  // Stop can be requested before getting this far.
  if (status_ == STOPPING)
    return;
  DCHECK(status_ == STARTING);
  FOR_EACH_OBSERVER(Listener, listener_list_, OnPausedAfterDownload());
}

bool EmbeddedWorkerInstance::OnMessageReceived(const IPC::Message& message) {
  ListenerList::Iterator it(&listener_list_);
  while (Listener* listener = it.GetNext()) {
    if (listener->OnMessageReceived(message))
      return true;
  }
  return false;
}

void EmbeddedWorkerInstance::OnReportException(
    const base::string16& error_message,
    int line_number,
    int column_number,
    const GURL& source_url) {
  FOR_EACH_OBSERVER(
      Listener,
      listener_list_,
      OnReportException(error_message, line_number, column_number, source_url));
}

void EmbeddedWorkerInstance::OnReportConsoleMessage(
    int source_identifier,
    int message_level,
    const base::string16& message,
    int line_number,
    const GURL& source_url) {
  FOR_EACH_OBSERVER(
      Listener,
      listener_list_,
      OnReportConsoleMessage(
          source_identifier, message_level, message, line_number, source_url));
}

int EmbeddedWorkerInstance::worker_devtools_agent_route_id() const {
  if (devtools_proxy_)
    return devtools_proxy_->agent_route_id();
  return MSG_ROUTING_NONE;
}

MessagePortMessageFilter* EmbeddedWorkerInstance::message_port_message_filter()
    const {
  return registry_->MessagePortMessageFilterForProcess(process_id_);
}

void EmbeddedWorkerInstance::AddListener(Listener* listener) {
  listener_list_.AddObserver(listener);
}

void EmbeddedWorkerInstance::RemoveListener(Listener* listener) {
  listener_list_.RemoveObserver(listener);
}

void EmbeddedWorkerInstance::OnNetworkAccessedForScriptLoad() {
  starting_phase_ = SCRIPT_DOWNLOADING;
  network_accessed_for_script_ = true;
}

// static
std::string EmbeddedWorkerInstance::StatusToString(Status status) {
  switch (status) {
    case STOPPED:
      return "STOPPED";
    case STARTING:
      return "STARTING";
    case RUNNING:
      return "RUNNING";
    case STOPPING:
      return "STOPPING";
  }
  NOTREACHED() << status;
  return std::string();
}

// static
std::string EmbeddedWorkerInstance::StartingPhaseToString(StartingPhase phase) {
  switch (phase) {
    case NOT_STARTING:
      return "Not in STARTING status";
    case ALLOCATING_PROCESS:
      return "Allocating process";
    case REGISTERING_TO_DEVTOOLS:
      return "Registering to DevTools";
    case SENT_START_WORKER:
      return "Sent StartWorker message to renderer";
    case SCRIPT_DOWNLOADING:
      return "Script downloading";
    case SCRIPT_LOADED:
      return "Script loaded";
    case SCRIPT_EVALUATED:
      return "Script evaluated";
    case STARTING_PHASE_MAX_VALUE:
      NOTREACHED();
  }
  NOTREACHED() << phase;
  return std::string();
}

}  // namespace content
