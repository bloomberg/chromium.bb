// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/embedded_worker_instance.h"

#include "base/bind_helpers.h"
#include "base/metrics/histogram_macros.h"
#include "base/threading/non_thread_safe.h"
#include "base/trace_event/trace_event.h"
#include "content/browser/devtools/service_worker_devtools_manager.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/common/content_switches_internal.h"
#include "content/common/mojo/service_registry_impl.h"
#include "content/common/service_worker/embedded_worker_messages.h"
#include "content/common/service_worker/embedded_worker_setup.mojom.h"
#include "content/common/service_worker/service_worker_types.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"
#include "ipc/ipc_message.h"
#include "url/gurl.h"

namespace content {

namespace {

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

void NotifyWorkerStopIgnoredOnUI(int worker_process_id, int worker_route_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
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
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
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

void SetupMojoOnUIThread(
    int process_id,
    int thread_id,
    mojo::InterfaceRequest<mojo::ServiceProvider> services,
    mojo::InterfacePtrInfo<mojo::ServiceProvider> exposed_services) {
  RenderProcessHost* rph = RenderProcessHost::FromID(process_id);
  // |rph| or its ServiceRegistry may be NULL in unit tests.
  if (!rph || !rph->GetServiceRegistry())
    return;
  EmbeddedWorkerSetupPtr setup;
  rph->GetServiceRegistry()->ConnectToRemoteService(mojo::GetProxy(&setup));
  setup->ExchangeServiceProviders(thread_id, services.Pass(),
                                  mojo::MakeProxy(exposed_services.Pass()));
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
  DCHECK(status_ == STOPPING || status_ == STOPPED) << status_;
  devtools_proxy_.reset();
  if (context_ && process_id_ != -1)
    context_->process_manager()->ReleaseWorkerProcess(embedded_worker_id_);
  if (registry_->GetWorker(embedded_worker_id_))
    registry_->RemoveWorker(process_id_, embedded_worker_id_);
}

void EmbeddedWorkerInstance::Start(int64 service_worker_version_id,
                                   const GURL& scope,
                                   const GURL& script_url,
                                   const StatusCallback& callback) {
  if (!context_) {
    callback.Run(SERVICE_WORKER_ERROR_ABORT);
    // |this| may be destroyed by the callback.
    return;
  }
  DCHECK(status_ == STOPPED);
  // TODO(horo): If we will see crashes here, we have to find the root cause of
  // the invalid version ID. Otherwise change CHECK to DCHECK.
  CHECK_NE(service_worker_version_id, kInvalidServiceWorkerVersionId);
  start_timing_ = base::TimeTicks::Now();
  status_ = STARTING;
  starting_phase_ = ALLOCATING_PROCESS;
  network_accessed_for_script_ = false;
  service_registry_.reset(new ServiceRegistryImpl());
  FOR_EACH_OBSERVER(Listener, listener_list_, OnStarting());
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
  UMA_HISTOGRAM_ENUMERATION("ServiceWorker.SendStopWorker.Status", status,
                            SERVICE_WORKER_ERROR_MAX_VALUE);
  // StopWorker could fail if we were starting up and don't have a process yet,
  // or we can no longer communicate with the process. So just detach.
  if (status != SERVICE_WORKER_OK) {
    OnDetached();
    return status;
  }

  status_ = STOPPING;
  FOR_EACH_OBSERVER(Listener, listener_list_, OnStopping());
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

ServiceWorkerStatusCode EmbeddedWorkerInstance::SendMessage(
    const IPC::Message& message) {
  DCHECK_NE(kInvalidEmbeddedWorkerThreadId, thread_id_);
  if (status_ != RUNNING && status_ != STARTING)
    return SERVICE_WORKER_ERROR_IPC_FAILED;
  return registry_->Send(process_id_,
                         new EmbeddedWorkerContextMsg_MessageToWorker(
                             thread_id_, embedded_worker_id_, message));
}

ServiceRegistry* EmbeddedWorkerInstance::GetServiceRegistry() {
  DCHECK(status_ == STARTING || status_ == RUNNING) << status_;
  return service_registry_.get();
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
    int process_id,
    bool is_new_process) {
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
  instance->ProcessAllocated(params.Pass(), callback, process_id,
                             is_new_process, status);
}

void EmbeddedWorkerInstance::ProcessAllocated(
    scoped_ptr<EmbeddedWorkerMsg_StartWorker_Params> params,
    const StatusCallback& callback,
    int process_id,
    bool is_new_process,
    ServiceWorkerStatusCode status) {
  DCHECK_EQ(process_id_, -1);
  TRACE_EVENT_ASYNC_END1("ServiceWorker",
                         "EmbeddedWorkerInstance::ProcessAllocate",
                         params.get(),
                         "Status", status);
  if (status != SERVICE_WORKER_OK) {
    OnStartFailed(callback, status);
    return;
  }
  const int64 service_worker_version_id = params->service_worker_version_id;
  process_id_ = process_id;
  GURL script_url(params->script_url);

  // Register this worker to DevToolsManager on UI thread, then continue to
  // call SendStartWorker on IO thread.
  starting_phase_ = REGISTERING_TO_DEVTOOLS;
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(RegisterToWorkerDevToolsManagerOnUI, process_id_,
                 context_.get(), context_, service_worker_version_id,
                 script_url,
                 base::Bind(&EmbeddedWorkerInstance::SendStartWorker,
                            weak_factory_.GetWeakPtr(), base::Passed(&params),
                            callback, is_new_process)));
}

void EmbeddedWorkerInstance::SendStartWorker(
    scoped_ptr<EmbeddedWorkerMsg_StartWorker_Params> params,
    const StatusCallback& callback,
    bool is_new_process,
    int worker_devtools_agent_route_id,
    bool wait_for_debugger) {
  // We may have been detached or stopped at some point during the start up
  // process, making process_id_ and other state invalid. If that happened,
  // abort instead of trying to send the IPC.
  if (status_ != STARTING) {
    OnStartFailed(callback, SERVICE_WORKER_ERROR_ABORT);
    return;
  }

  if (worker_devtools_agent_route_id != MSG_ROUTING_NONE) {
    DCHECK(!devtools_proxy_);
    devtools_proxy_.reset(new DevToolsProxy(process_id_,
                                            worker_devtools_agent_route_id));
  }
  params->worker_devtools_agent_route_id = worker_devtools_agent_route_id;
  params->wait_for_debugger = wait_for_debugger;
  if (params->wait_for_debugger) {
    // We don't measure the start time when wait_for_debugger flag is set. So we
    // set the NULL time here.
    start_timing_ = base::TimeTicks();
  } else {
    DCHECK(!start_timing_.is_null());
    if (is_new_process) {
      UMA_HISTOGRAM_TIMES("EmbeddedWorkerInstance.NewProcessAllocation",
                          base::TimeTicks::Now() - start_timing_);
    } else {
      UMA_HISTOGRAM_TIMES("EmbeddedWorkerInstance.ExistingProcessAllocation",
                          base::TimeTicks::Now() - start_timing_);
    }
    UMA_HISTOGRAM_BOOLEAN("EmbeddedWorkerInstance.ProcessCreated",
                          is_new_process);
    // Reset |start_timing_| to measure the time excluding the process
    // allocation time.
    start_timing_ = base::TimeTicks::Now();
  }

  starting_phase_ = SENT_START_WORKER;
  ServiceWorkerStatusCode status =
      registry_->SendStartWorker(params.Pass(), process_id_);
  if (status != SERVICE_WORKER_OK) {
    OnStartFailed(callback, status);
    return;
  }
  DCHECK(start_callback_.is_null());
  start_callback_ = callback;
}

void EmbeddedWorkerInstance::OnReadyForInspection() {
  if (devtools_proxy_)
    devtools_proxy_->NotifyWorkerReadyForInspection();
}

void EmbeddedWorkerInstance::OnScriptReadStarted() {
  starting_phase_ = SCRIPT_READ_STARTED;
}

void EmbeddedWorkerInstance::OnScriptReadFinished() {
  starting_phase_ = SCRIPT_READ_FINISHED;
}

void EmbeddedWorkerInstance::OnScriptLoaded() {
  FOR_EACH_OBSERVER(Listener, listener_list_, OnScriptLoaded());
  starting_phase_ = SCRIPT_LOADED;
}

void EmbeddedWorkerInstance::OnThreadStarted(int thread_id) {
  starting_phase_ = THREAD_STARTED;
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
  FOR_EACH_OBSERVER(Listener, listener_list_, OnThreadStarted());

  mojo::ServiceProviderPtr exposed_services;
  service_registry_->Bind(GetProxy(&exposed_services));
  mojo::ServiceProviderPtr services;
  mojo::InterfaceRequest<mojo::ServiceProvider> services_request =
      GetProxy(&services);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(SetupMojoOnUIThread, process_id_, thread_id_,
                 base::Passed(&services_request),
                 base::Passed(exposed_services.PassInterface())));
  service_registry_->BindRemoteServiceProvider(services.Pass());
}

void EmbeddedWorkerInstance::OnScriptLoadFailed() {
  FOR_EACH_OBSERVER(Listener, listener_list_, OnScriptLoadFailed());
}

void EmbeddedWorkerInstance::OnScriptEvaluated(bool success) {
  starting_phase_ = SCRIPT_EVALUATED;
  if (start_callback_.is_null()) {
    DVLOG(1) << "Received unexpected OnScriptEvaluated message.";
    return;
  }
  if (success && !start_timing_.is_null()) {
    UMA_HISTOGRAM_TIMES("EmbeddedWorkerInstance.ScriptEvaluate",
                        base::TimeTicks::Now() - start_timing_);
  }
  StatusCallback callback = start_callback_;
  start_callback_.Reset();
  callback.Run(success ? SERVICE_WORKER_OK
                       : SERVICE_WORKER_ERROR_SCRIPT_EVALUATE_FAILED);
  // |this| may be destroyed by the callback.
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
  Status old_status = status_;
  ReleaseProcess();
  FOR_EACH_OBSERVER(Listener, listener_list_, OnStopped(old_status));
}

void EmbeddedWorkerInstance::OnDetached() {
  Status old_status = status_;
  ReleaseProcess();
  FOR_EACH_OBSERVER(Listener, listener_list_, OnDetached(old_status));
}

void EmbeddedWorkerInstance::Detach() {
  registry_->RemoveWorker(process_id_, embedded_worker_id_);
  OnDetached();
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

void EmbeddedWorkerInstance::ReleaseProcess() {
  devtools_proxy_.reset();
  if (context_ && process_id_ != -1)
    context_->process_manager()->ReleaseWorkerProcess(embedded_worker_id_);
  status_ = STOPPED;
  process_id_ = -1;
  thread_id_ = -1;
  service_registry_.reset();
  start_callback_.Reset();
}

void EmbeddedWorkerInstance::OnStartFailed(const StatusCallback& callback,
                                           ServiceWorkerStatusCode status) {
  Status old_status = status_;
  ReleaseProcess();
  base::WeakPtr<EmbeddedWorkerInstance> weak_this = weak_factory_.GetWeakPtr();
  callback.Run(status);
  if (weak_this && old_status != STOPPED)
    FOR_EACH_OBSERVER(Listener, weak_this->listener_list_,
                      OnStopped(old_status));
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
    case THREAD_STARTED:
      return "Thread started";
    case SCRIPT_READ_STARTED:
      return "Script read started";
    case SCRIPT_READ_FINISHED:
      return "Script read finished";
    case STARTING_PHASE_MAX_VALUE:
      NOTREACHED();
  }
  NOTREACHED() << phase;
  return std::string();
}

}  // namespace content
