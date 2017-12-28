// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/service_worker_devtools_agent_host.h"

#include "base/strings/stringprintf.h"
#include "content/browser/devtools/devtools_session.h"
#include "content/browser/devtools/protocol/inspector_handler.h"
#include "content/browser/devtools/protocol/network_handler.h"
#include "content/browser/devtools/protocol/protocol.h"
#include "content/browser/devtools/protocol/schema_handler.h"
#include "content/browser/devtools/service_worker_devtools_manager.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"

namespace content {

namespace {

void TerminateServiceWorkerOnIO(
    base::WeakPtr<ServiceWorkerContextCore> context_weak,
    int64_t version_id) {
  if (ServiceWorkerContextCore* context = context_weak.get()) {
    if (ServiceWorkerVersion* version = context->GetLiveVersion(version_id))
      version->StopWorker(base::BindOnce(&base::DoNothing));
  }
}

void SetDevToolsAttachedOnIO(
    base::WeakPtr<ServiceWorkerContextCore> context_weak,
    int64_t version_id,
    bool attached) {
  if (ServiceWorkerContextCore* context = context_weak.get()) {
    if (ServiceWorkerVersion* version = context->GetLiveVersion(version_id))
      version->SetDevToolsAttached(attached);
  }
}

}  // namespace

ServiceWorkerDevToolsAgentHost::ServiceWorkerDevToolsAgentHost(
    int worker_process_id,
    int worker_route_id,
    const ServiceWorkerIdentifier& service_worker,
    bool is_installed_version)
    : DevToolsAgentHostImpl(service_worker.devtools_worker_token().ToString()),
      state_(WORKER_UNINSPECTED),
      worker_process_id_(worker_process_id),
      worker_route_id_(worker_route_id),
      service_worker_(new ServiceWorkerIdentifier(service_worker)),
      version_installed_time_(is_installed_version ? base::Time::Now()
                                                   : base::Time()) {
  NotifyCreated();
}

BrowserContext* ServiceWorkerDevToolsAgentHost::GetBrowserContext() {
  RenderProcessHost* rph = RenderProcessHost::FromID(worker_process_id_);
  return rph ? rph->GetBrowserContext() : nullptr;
}

std::string ServiceWorkerDevToolsAgentHost::GetType() {
  return kTypeServiceWorker;
}

std::string ServiceWorkerDevToolsAgentHost::GetTitle() {
  return "Service Worker " + service_worker_->url().spec();
}

GURL ServiceWorkerDevToolsAgentHost::GetURL() {
  return service_worker_->url();
}

bool ServiceWorkerDevToolsAgentHost::Activate() {
  return false;
}

void ServiceWorkerDevToolsAgentHost::Reload() {
}

bool ServiceWorkerDevToolsAgentHost::Close() {
  BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
                          base::BindOnce(&TerminateServiceWorkerOnIO,
                                         service_worker_->context_weak(),
                                         service_worker_->version_id()));
  return true;
}

void ServiceWorkerDevToolsAgentHost::WorkerVersionInstalled() {
  version_installed_time_ = base::Time::Now();
}

void ServiceWorkerDevToolsAgentHost::WorkerVersionDoomed() {
  version_doomed_time_ = base::Time::Now();
}

GURL ServiceWorkerDevToolsAgentHost::scope() const {
  return service_worker_->scope();
}

bool ServiceWorkerDevToolsAgentHost::Matches(
    const ServiceWorkerIdentifier& other) {
  return service_worker_->Matches(other);
}

ServiceWorkerDevToolsAgentHost::~ServiceWorkerDevToolsAgentHost() {
  ServiceWorkerDevToolsManager::GetInstance()->AgentHostDestroyed(this);
}

void ServiceWorkerDevToolsAgentHost::AttachSession(DevToolsSession* session) {
  if (state_ != WORKER_INSPECTED) {
    state_ = WORKER_INSPECTED;
    AttachToWorker();
  }
  if (RenderProcessHost* host = RenderProcessHost::FromID(worker_process_id_)) {
    session->SetRenderer(host, nullptr);
    host->Send(
        new DevToolsAgentMsg_Attach(worker_route_id_, session->session_id()));
  }
  session->SetFallThroughForNotFound(true);
  session->AddHandler(base::WrapUnique(new protocol::InspectorHandler()));
  session->AddHandler(base::WrapUnique(new protocol::NetworkHandler(GetId())));
  session->AddHandler(base::WrapUnique(new protocol::SchemaHandler()));
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&SetDevToolsAttachedOnIO, service_worker_->context_weak(),
                     service_worker_->version_id(), true));
}

void ServiceWorkerDevToolsAgentHost::DetachSession(int session_id) {
  if (RenderProcessHost* host = RenderProcessHost::FromID(worker_process_id_))
    host->Send(new DevToolsAgentMsg_Detach(worker_route_id_, session_id));
  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      base::BindOnce(&SetDevToolsAttachedOnIO, service_worker_->context_weak(),
                     service_worker_->version_id(), false));
  if (state_ == WORKER_INSPECTED) {
    state_ = WORKER_UNINSPECTED;
    DetachFromWorker();
  } else if (state_ == WORKER_PAUSED_FOR_REATTACH) {
    state_ = WORKER_UNINSPECTED;
  }
}

bool ServiceWorkerDevToolsAgentHost::DispatchProtocolMessage(
    DevToolsSession* session,
    const std::string& message) {
  if (state_ != WORKER_INSPECTED)
    return true;

  int call_id = 0;
  std::string method;
  if (session->Dispatch(message, &call_id, &method) !=
      protocol::Response::kFallThrough) {
    return true;
  }

  if (RenderProcessHost* host = RenderProcessHost::FromID(worker_process_id_)) {
    host->Send(new DevToolsAgentMsg_DispatchOnInspectorBackend(
        worker_route_id_, session->session_id(), call_id, method, message));
    session->waiting_messages()[call_id] = {method, message};
  }
  return true;
}

bool ServiceWorkerDevToolsAgentHost::OnMessageReceived(
    const IPC::Message& msg) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(ServiceWorkerDevToolsAgentHost, msg)
    IPC_MESSAGE_HANDLER(DevToolsClientMsg_DispatchOnInspectorFrontend,
                        OnDispatchOnInspectorFrontend)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void ServiceWorkerDevToolsAgentHost::PauseForDebugOnStart() {
  DCHECK(state_ == WORKER_UNINSPECTED);
  state_ = WORKER_PAUSED_FOR_DEBUG_ON_START;
}

bool ServiceWorkerDevToolsAgentHost::IsPausedForDebugOnStart() {
  return state_ == WORKER_PAUSED_FOR_DEBUG_ON_START ||
         state_ == WORKER_READY_FOR_DEBUG_ON_START;
}

bool ServiceWorkerDevToolsAgentHost::IsReadyForInspection() {
  return state_ == WORKER_INSPECTED || state_ == WORKER_UNINSPECTED ||
         state_ == WORKER_READY_FOR_DEBUG_ON_START;
}

void ServiceWorkerDevToolsAgentHost::WorkerReadyForInspection() {
  if (state_ == WORKER_PAUSED_FOR_REATTACH) {
    DCHECK(IsAttached());
    state_ = WORKER_INSPECTED;
    AttachToWorker();
    if (RenderProcessHost* host =
            RenderProcessHost::FromID(worker_process_id_)) {
      for (DevToolsSession* session : sessions()) {
        session->SetRenderer(host, nullptr);
        host->Send(new DevToolsAgentMsg_Reattach(
            worker_route_id_, session->session_id(), session->state_cookie()));
        for (const auto& pair : session->waiting_messages()) {
          int call_id = pair.first;
          const DevToolsSession::Message& message = pair.second;
          host->Send(new DevToolsAgentMsg_DispatchOnInspectorBackend(
              worker_route_id_, session->session_id(), call_id, message.method,
              message.message));
        }
      }
    }
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        base::BindOnce(&SetDevToolsAttachedOnIO,
                       service_worker_->context_weak(),
                       service_worker_->version_id(), true));
  } else if (state_ == WORKER_PAUSED_FOR_DEBUG_ON_START) {
    state_ = WORKER_READY_FOR_DEBUG_ON_START;
  }
}

void ServiceWorkerDevToolsAgentHost::WorkerRestarted(int worker_process_id,
                                                     int worker_route_id) {
  DCHECK_EQ(WORKER_TERMINATED, state_);
  state_ = IsAttached() ? WORKER_PAUSED_FOR_REATTACH : WORKER_UNINSPECTED;
  worker_process_id_ = worker_process_id;
  worker_route_id_ = worker_route_id;
  RenderProcessHost* host = RenderProcessHost::FromID(worker_process_id_);
  for (DevToolsSession* session : sessions())
    session->SetRenderer(host, nullptr);
}

void ServiceWorkerDevToolsAgentHost::WorkerDestroyed() {
  DCHECK_NE(WORKER_TERMINATED, state_);
  if (state_ == WORKER_INSPECTED) {
    DCHECK(IsAttached());
    for (auto* inspector : protocol::InspectorHandler::ForAgentHost(this))
      inspector->TargetCrashed();
    DetachFromWorker();
    for (DevToolsSession* session : sessions())
      session->SetRenderer(nullptr, nullptr);
  }
  state_ = WORKER_TERMINATED;
}

void ServiceWorkerDevToolsAgentHost::AttachToWorker() {
  if (RenderProcessHost* host = RenderProcessHost::FromID(worker_process_id_))
    host->AddRoute(worker_route_id_, this);
}

void ServiceWorkerDevToolsAgentHost::DetachFromWorker() {
  if (RenderProcessHost* host = RenderProcessHost::FromID(worker_process_id_))
    host->RemoveRoute(worker_route_id_);
}

void ServiceWorkerDevToolsAgentHost::OnDispatchOnInspectorFrontend(
    const DevToolsMessageChunk& message) {
  DevToolsSession* session = SessionById(message.session_id);
  if (session)
    session->ReceiveMessageChunk(message);
}

}  // namespace content
