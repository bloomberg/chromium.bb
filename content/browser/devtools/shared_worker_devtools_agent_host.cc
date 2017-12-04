// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/shared_worker_devtools_agent_host.h"

#include "content/browser/devtools/devtools_session.h"
#include "content/browser/devtools/protocol/inspector_handler.h"
#include "content/browser/devtools/protocol/network_handler.h"
#include "content/browser/devtools/protocol/protocol.h"
#include "content/browser/devtools/protocol/schema_handler.h"
#include "content/browser/devtools/shared_worker_devtools_manager.h"
#include "content/browser/shared_worker/shared_worker_host.h"
#include "content/browser/shared_worker/shared_worker_instance.h"
#include "content/browser/shared_worker/shared_worker_service_impl.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"

namespace content {

SharedWorkerDevToolsAgentHost::SharedWorkerDevToolsAgentHost(
    SharedWorkerHost* worker_host)
    : DevToolsAgentHostImpl(
          worker_host->instance()->devtools_worker_token().ToString()),
      worker_host_(worker_host),
      instance_(new SharedWorkerInstance(*worker_host->instance())) {
  NotifyCreated();
}

SharedWorkerDevToolsAgentHost::~SharedWorkerDevToolsAgentHost() {
  SharedWorkerDevToolsManager::GetInstance()->AgentHostDestroyed(this);
}

BrowserContext* SharedWorkerDevToolsAgentHost::GetBrowserContext() {
  RenderProcessHost* rph = GetProcess();
  return rph ? rph->GetBrowserContext() : nullptr;
}

std::string SharedWorkerDevToolsAgentHost::GetType() {
  return kTypeSharedWorker;
}

std::string SharedWorkerDevToolsAgentHost::GetTitle() {
  return instance_->name();
}

GURL SharedWorkerDevToolsAgentHost::GetURL() {
  return instance_->url();
}

bool SharedWorkerDevToolsAgentHost::Activate() {
  return false;
}

void SharedWorkerDevToolsAgentHost::Reload() {
}

bool SharedWorkerDevToolsAgentHost::Close() {
  if (worker_host_)
    worker_host_->TerminateWorker();
  return true;
}

void SharedWorkerDevToolsAgentHost::AttachSession(DevToolsSession* session) {
  if (RenderProcessHost* host = GetProcess()) {
    if (sessions().size() == 1)
      host->AddRoute(worker_host_->route_id(), this);
    session->SetRenderer(host, nullptr);
    if (!waiting_ready_for_reattach_) {
      host->Send(new DevToolsAgentMsg_Attach(worker_host_->route_id(),
                                             session->session_id()));
    }
  }
  session->SetFallThroughForNotFound(true);
  session->AddHandler(std::make_unique<protocol::InspectorHandler>());
  session->AddHandler(std::make_unique<protocol::NetworkHandler>(GetId()));
  session->AddHandler(std::make_unique<protocol::SchemaHandler>());
}

void SharedWorkerDevToolsAgentHost::DetachSession(int session_id) {
  if (RenderProcessHost* host = GetProcess()) {
    host->Send(
        new DevToolsAgentMsg_Detach(worker_host_->route_id(), session_id));
    if (!sessions().size())
      host->RemoveRoute(worker_host_->route_id());
  }
}

bool SharedWorkerDevToolsAgentHost::DispatchProtocolMessage(
    DevToolsSession* session,
    const std::string& message) {
  int call_id = 0;
  std::string method;
  if (session->Dispatch(message, &call_id, &method) !=
      protocol::Response::kFallThrough) {
    return true;
  }

  if (RenderProcessHost* host = GetProcess()) {
    host->Send(new DevToolsAgentMsg_DispatchOnInspectorBackend(
        worker_host_->route_id(), session->session_id(), call_id, method,
        message));
    session->waiting_messages()[call_id] = {method, message};
  }
  return true;
}

bool SharedWorkerDevToolsAgentHost::OnMessageReceived(const IPC::Message& msg) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SharedWorkerDevToolsAgentHost, msg)
    IPC_MESSAGE_HANDLER(DevToolsClientMsg_DispatchOnInspectorFrontend,
                        OnDispatchOnInspectorFrontend)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

bool SharedWorkerDevToolsAgentHost::Matches(SharedWorkerHost* worker_host) {
  return instance_->Matches(*worker_host->instance());
}

void SharedWorkerDevToolsAgentHost::WorkerReadyForInspection() {
  DCHECK(worker_host_);
  if (!waiting_ready_for_reattach_)
    return;
  waiting_ready_for_reattach_ = false;
  if (RenderProcessHost* host = GetProcess()) {
    for (DevToolsSession* session : sessions()) {
      host->Send(new DevToolsAgentMsg_Reattach(worker_host_->route_id(),
                                               session->session_id(),
                                               session->state_cookie()));
      for (const auto& pair : session->waiting_messages()) {
        int call_id = pair.first;
        const DevToolsSession::Message& message = pair.second;
        host->Send(new DevToolsAgentMsg_DispatchOnInspectorBackend(
            worker_host_->route_id(), session->session_id(), call_id,
            message.method, message.message));
      }
    }
  }
}

bool SharedWorkerDevToolsAgentHost::WorkerRestarted(
    SharedWorkerHost* worker_host) {
  DCHECK(!worker_host_);
  worker_host_ = worker_host;
  if (RenderProcessHost* host = GetProcess()) {
    if (sessions().size())
      host->AddRoute(worker_host_->route_id(), this);
    for (DevToolsSession* session : sessions())
      session->SetRenderer(host, nullptr);
  }
  waiting_ready_for_reattach_ = IsAttached();
  return waiting_ready_for_reattach_;
}

void SharedWorkerDevToolsAgentHost::WorkerDestroyed() {
  DCHECK(worker_host_);
  for (auto* inspector : protocol::InspectorHandler::ForAgentHost(this))
    inspector->TargetCrashed();
  for (DevToolsSession* session : sessions())
    session->SetRenderer(nullptr, nullptr);
  if (sessions().size()) {
    if (RenderProcessHost* host = GetProcess())
      host->RemoveRoute(worker_host_->route_id());
  }
  worker_host_ = nullptr;
}

RenderProcessHost* SharedWorkerDevToolsAgentHost::GetProcess() {
  return worker_host_ ? RenderProcessHost::FromID(worker_host_->process_id())
                      : nullptr;
}

void SharedWorkerDevToolsAgentHost::OnDispatchOnInspectorFrontend(
    const DevToolsMessageChunk& message) {
  DevToolsSession* session = SessionById(message.session_id);
  if (session)
    session->ReceiveMessageChunk(message);
}

}  // namespace content
