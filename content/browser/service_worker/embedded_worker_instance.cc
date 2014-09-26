// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/service_worker/embedded_worker_instance.h"

#include <algorithm>
#include <utility>

#include "base/bind_helpers.h"
#include "base/debug/trace_event.h"
#include "content/browser/devtools/embedded_worker_devtools_manager.h"
#include "content/browser/service_worker/embedded_worker_registry.h"
#include "content/browser/service_worker/service_worker_context_core.h"
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

void NotifyWorkerReadyForInspection(int worker_process_id,
                                    int worker_route_id) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            base::Bind(NotifyWorkerReadyForInspection,
                                       worker_process_id,
                                       worker_route_id));
    return;
  }
  EmbeddedWorkerDevToolsManager::GetInstance()->WorkerReadyForInspection(
      worker_process_id, worker_route_id);
}

void NotifyWorkerContextStarted(int worker_process_id, int worker_route_id) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(
            NotifyWorkerContextStarted, worker_process_id, worker_route_id));
    return;
  }
  EmbeddedWorkerDevToolsManager::GetInstance()->WorkerContextStarted(
      worker_process_id, worker_route_id);
}

void NotifyWorkerDestroyed(int worker_process_id, int worker_route_id) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(
        BrowserThread::UI,
        FROM_HERE,
        base::Bind(NotifyWorkerDestroyed, worker_process_id, worker_route_id));
    return;
  }
  EmbeddedWorkerDevToolsManager::GetInstance()->WorkerDestroyed(
      worker_process_id, worker_route_id);
}

void RegisterToWorkerDevToolsManager(
    int process_id,
    const ServiceWorkerContextCore* service_worker_context,
    base::WeakPtr<ServiceWorkerContextCore> service_worker_context_weak,
    int64 service_worker_version_id,
    const GURL& url,
    const base::Callback<void(int worker_devtools_agent_route_id,
                              bool wait_for_debugger)>& callback) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::UI)) {
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            base::Bind(RegisterToWorkerDevToolsManager,
                                       process_id,
                                       service_worker_context,
                                       service_worker_context_weak,
                                       service_worker_version_id,
                                       url,
                                       callback));
    return;
  }
  int worker_devtools_agent_route_id = MSG_ROUTING_NONE;
  bool wait_for_debugger = false;
  if (RenderProcessHost* rph = RenderProcessHost::FromID(process_id)) {
    // |rph| may be NULL in unit tests.
    worker_devtools_agent_route_id = rph->GetNextRoutingID();
    wait_for_debugger =
        EmbeddedWorkerDevToolsManager::GetInstance()->ServiceWorkerCreated(
            process_id,
            worker_devtools_agent_route_id,
            EmbeddedWorkerDevToolsManager::ServiceWorkerIdentifier(
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

EmbeddedWorkerInstance::~EmbeddedWorkerInstance() {
  if (status_ == STARTING || status_ == RUNNING)
    Stop();
  if (worker_devtools_agent_route_id_ != MSG_ROUTING_NONE)
    NotifyWorkerDestroyed(process_id_, worker_devtools_agent_route_id_);
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
  status_ = STARTING;
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
  DCHECK(status_ == STARTING || status_ == RUNNING);
  ServiceWorkerStatusCode status =
      registry_->StopWorker(process_id_, embedded_worker_id_);
  if (status == SERVICE_WORKER_OK)
    status_ = STOPPING;
  return status;
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
      process_id_(-1),
      thread_id_(kInvalidEmbeddedWorkerThreadId),
      worker_devtools_agent_route_id_(MSG_ROUTING_NONE),
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
  RegisterToWorkerDevToolsManager(
      process_id,
      context_.get(),
      context_,
      service_worker_version_id,
      script_url,
      base::Bind(&EmbeddedWorkerInstance::SendStartWorker,
                 weak_factory_.GetWeakPtr(),
                 base::Passed(&params),
                 callback));
}

void EmbeddedWorkerInstance::SendStartWorker(
    scoped_ptr<EmbeddedWorkerMsg_StartWorker_Params> params,
    const StatusCallback& callback,
    int worker_devtools_agent_route_id,
    bool wait_for_debugger) {
  worker_devtools_agent_route_id_ = worker_devtools_agent_route_id;
  params->worker_devtools_agent_route_id = worker_devtools_agent_route_id;
  params->wait_for_debugger = wait_for_debugger;
  registry_->SendStartWorker(params.Pass(), callback, process_id_);
}

void EmbeddedWorkerInstance::OnReadyForInspection() {
  if (worker_devtools_agent_route_id_ != MSG_ROUTING_NONE)
    NotifyWorkerReadyForInspection(process_id_,
                                   worker_devtools_agent_route_id_);
}

void EmbeddedWorkerInstance::OnScriptLoaded(int thread_id) {
  thread_id_ = thread_id;
  if (worker_devtools_agent_route_id_ != MSG_ROUTING_NONE)
    NotifyWorkerContextStarted(process_id_, worker_devtools_agent_route_id_);
}

void EmbeddedWorkerInstance::OnScriptLoadFailed() {
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
  if (worker_devtools_agent_route_id_ != MSG_ROUTING_NONE)
    NotifyWorkerDestroyed(process_id_, worker_devtools_agent_route_id_);
  if (context_)
    context_->process_manager()->ReleaseWorkerProcess(embedded_worker_id_);
  status_ = STOPPED;
  process_id_ = -1;
  thread_id_ = -1;
  worker_devtools_agent_route_id_ = MSG_ROUTING_NONE;
  FOR_EACH_OBSERVER(Listener, listener_list_, OnStopped());
}

void EmbeddedWorkerInstance::OnPausedAfterDownload() {
  // Stop can be requested before getting this far.
  if (status_ == STOPPING)
    return;
  DCHECK(status_ == STARTING);
  FOR_EACH_OBSERVER(Listener, listener_list_, OnPausedAfterDownload());
}

bool EmbeddedWorkerInstance::OnMessageReceived(const IPC::Message& message) {
  ListenerList::Iterator it(listener_list_);
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

void EmbeddedWorkerInstance::AddListener(Listener* listener) {
  listener_list_.AddObserver(listener);
}

void EmbeddedWorkerInstance::RemoveListener(Listener* listener) {
  listener_list_.RemoveObserver(listener);
}

}  // namespace content
