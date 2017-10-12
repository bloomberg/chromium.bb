// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/worker_devtools_agent_host.h"

#include "base/guid.h"
#include "base/memory/ptr_util.h"
#include "content/browser/devtools/devtools_session.h"
#include "content/browser/devtools/protocol/inspector_handler.h"
#include "content/browser/devtools/protocol/network_handler.h"
#include "content/browser/devtools/protocol/protocol.h"
#include "content/browser/devtools/protocol/schema_handler.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"

namespace content {

BrowserContext* WorkerDevToolsAgentHost::GetBrowserContext() {
  RenderProcessHost* rph = RenderProcessHost::FromID(worker_id_.first);
  return rph ? rph->GetBrowserContext() : nullptr;
}

void WorkerDevToolsAgentHost::AttachSession(DevToolsSession* session) {
  if (state_ != WORKER_INSPECTED) {
    state_ = WORKER_INSPECTED;
    AttachToWorker();
  }
  if (RenderProcessHost* host = RenderProcessHost::FromID(worker_id_.first)) {
    session->SetRenderer(host, nullptr);
    host->Send(new DevToolsAgentMsg_Attach(
        worker_id_.second, GetId(), session->session_id()));
  }
  session->SetFallThroughForNotFound(true);
  session->AddHandler(base::WrapUnique(new protocol::InspectorHandler()));
  session->AddHandler(base::WrapUnique(new protocol::NetworkHandler(GetId())));
  session->AddHandler(base::WrapUnique(new protocol::SchemaHandler()));
  OnAttachedStateChanged(true);
}

void WorkerDevToolsAgentHost::DetachSession(int session_id) {
  if (RenderProcessHost* host = RenderProcessHost::FromID(worker_id_.first))
    host->Send(new DevToolsAgentMsg_Detach(worker_id_.second, session_id));
  OnAttachedStateChanged(false);
  if (state_ == WORKER_INSPECTED) {
    state_ = WORKER_UNINSPECTED;
    DetachFromWorker();
  } else if (state_ == WORKER_PAUSED_FOR_REATTACH) {
    state_ = WORKER_UNINSPECTED;
  }
}

bool WorkerDevToolsAgentHost::DispatchProtocolMessage(
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

  if (RenderProcessHost* host = RenderProcessHost::FromID(worker_id_.first)) {
    host->Send(new DevToolsAgentMsg_DispatchOnInspectorBackend(
        worker_id_.second, session->session_id(), call_id, method, message));
    session->waiting_messages()[call_id] = {method, message};
  }
  return true;
}

bool WorkerDevToolsAgentHost::OnMessageReceived(
    const IPC::Message& msg) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WorkerDevToolsAgentHost, msg)
  IPC_MESSAGE_HANDLER(DevToolsClientMsg_DispatchOnInspectorFrontend,
                      OnDispatchOnInspectorFrontend)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void WorkerDevToolsAgentHost::PauseForDebugOnStart() {
  DCHECK(state_ == WORKER_UNINSPECTED);
  state_ = WORKER_PAUSED_FOR_DEBUG_ON_START;
}

bool WorkerDevToolsAgentHost::IsPausedForDebugOnStart() {
  return state_ == WORKER_PAUSED_FOR_DEBUG_ON_START ||
         state_ == WORKER_READY_FOR_DEBUG_ON_START;
}

bool WorkerDevToolsAgentHost::IsReadyForInspection() {
  return state_ == WORKER_INSPECTED || state_ == WORKER_UNINSPECTED ||
         state_ == WORKER_READY_FOR_DEBUG_ON_START;
}

void WorkerDevToolsAgentHost::WorkerReadyForInspection() {
  if (state_ == WORKER_PAUSED_FOR_REATTACH) {
    DCHECK(IsAttached());
    state_ = WORKER_INSPECTED;
    AttachToWorker();
    if (RenderProcessHost* host = RenderProcessHost::FromID(worker_id_.first)) {
      for (DevToolsSession* session : sessions()) {
        session->SetRenderer(host, nullptr);
        host->Send(new DevToolsAgentMsg_Reattach(worker_id_.second, GetId(),
                                                 session->session_id(),
                                                 session->state_cookie()));
        for (const auto& pair : session->waiting_messages()) {
          int call_id = pair.first;
          const DevToolsSession::Message& message = pair.second;
          host->Send(new DevToolsAgentMsg_DispatchOnInspectorBackend(
              worker_id_.second, session->session_id(), call_id, message.method,
              message.message));
        }
      }
    }
    OnAttachedStateChanged(true);
  } else if (state_ == WORKER_PAUSED_FOR_DEBUG_ON_START) {
    state_ = WORKER_READY_FOR_DEBUG_ON_START;
  }
}

void WorkerDevToolsAgentHost::WorkerRestarted(WorkerId worker_id) {
  DCHECK_EQ(WORKER_TERMINATED, state_);
  state_ = IsAttached() ? WORKER_PAUSED_FOR_REATTACH : WORKER_UNINSPECTED;
  worker_id_ = worker_id;
  RenderProcessHost* host = RenderProcessHost::FromID(worker_id_.first);
  for (DevToolsSession* session : sessions())
    session->SetRenderer(host, nullptr);
  WorkerCreated();
}

void WorkerDevToolsAgentHost::WorkerDestroyed() {
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
  Release();  // Balanced in WorkerCreated().
}

bool WorkerDevToolsAgentHost::IsTerminated() {
  return state_ == WORKER_TERMINATED;
}

WorkerDevToolsAgentHost::WorkerDevToolsAgentHost(WorkerId worker_id)
    : DevToolsAgentHostImpl(base::GenerateGUID()),
      state_(WORKER_UNINSPECTED),
      worker_id_(worker_id) {
  WorkerCreated();
}

WorkerDevToolsAgentHost::~WorkerDevToolsAgentHost() {
  DCHECK_EQ(WORKER_TERMINATED, state_);
}

void WorkerDevToolsAgentHost::OnAttachedStateChanged(bool attached) {
}

void WorkerDevToolsAgentHost::AttachToWorker() {
  if (RenderProcessHost* host = RenderProcessHost::FromID(worker_id_.first))
    host->AddRoute(worker_id_.second, this);
}

void WorkerDevToolsAgentHost::DetachFromWorker() {
  if (RenderProcessHost* host = RenderProcessHost::FromID(worker_id_.first))
    host->RemoveRoute(worker_id_.second);
}

void WorkerDevToolsAgentHost::WorkerCreated() {
  AddRef();  // Balanced in WorkerDestroyed()
}

void WorkerDevToolsAgentHost::OnDispatchOnInspectorFrontend(
    const DevToolsMessageChunk& message) {
  if (!IsAttached())
    return;
  DevToolsSession* session = SessionById(message.session_id);
  if (session)
    session->ReceiveMessageChunk(message);
}

}  // namespace content
