// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/embedded_worker_devtools_agent_host.h"

#include "content/browser/devtools/protocol/devtools_protocol_handler.h"
#include "content/common/devtools_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"

namespace content {

bool EmbeddedWorkerDevToolsAgentHost::IsWorker() const {
  return true;
}

BrowserContext* EmbeddedWorkerDevToolsAgentHost::GetBrowserContext() {
  RenderProcessHost* rph = RenderProcessHost::FromID(worker_id_.first);
  return rph ? rph->GetBrowserContext() : nullptr;
}

void EmbeddedWorkerDevToolsAgentHost::SendMessageToAgent(
    IPC::Message* message_raw) {
  scoped_ptr<IPC::Message> message(message_raw);
  if (state_ != WORKER_INSPECTED)
    return;
  if (RenderProcessHost* host = RenderProcessHost::FromID(worker_id_.first)) {
    message->set_routing_id(worker_id_.second);
    host->Send(message.release());
  }
}

void EmbeddedWorkerDevToolsAgentHost::Attach() {
  if (state_ != WORKER_INSPECTED) {
    state_ = WORKER_INSPECTED;
    AttachToWorker();
  }
  IPCDevToolsAgentHost::Attach();
}

void EmbeddedWorkerDevToolsAgentHost::OnClientAttached() {
  DevToolsAgentHostImpl::NotifyCallbacks(this, true);
}

void EmbeddedWorkerDevToolsAgentHost::OnClientDetached() {
  if (state_ == WORKER_INSPECTED) {
    state_ = WORKER_UNINSPECTED;
    DetachFromWorker();
  } else if (state_ == WORKER_PAUSED_FOR_REATTACH) {
    state_ = WORKER_UNINSPECTED;
  }
  DevToolsAgentHostImpl::NotifyCallbacks(this, false);
}

bool EmbeddedWorkerDevToolsAgentHost::OnMessageReceived(
    const IPC::Message& msg) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(EmbeddedWorkerDevToolsAgentHost, msg)
  IPC_MESSAGE_HANDLER(DevToolsClientMsg_DispatchOnInspectorFrontend,
                      OnDispatchOnInspectorFrontend)
  IPC_MESSAGE_HANDLER(DevToolsHostMsg_SaveAgentRuntimeState,
                      OnSaveAgentRuntimeState)
  IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void EmbeddedWorkerDevToolsAgentHost::WorkerReadyForInspection() {
  if (state_ == WORKER_PAUSED_FOR_DEBUG_ON_START) {
    RenderProcessHost* rph = RenderProcessHost::FromID(worker_id_.first);
    Inspect(rph->GetBrowserContext());
  } else if (state_ == WORKER_PAUSED_FOR_REATTACH) {
    DCHECK(IsAttached());
    state_ = WORKER_INSPECTED;
    AttachToWorker();
    Reattach(saved_agent_state_);
  }
}

void EmbeddedWorkerDevToolsAgentHost::WorkerRestarted(WorkerId worker_id) {
  DCHECK_EQ(WORKER_TERMINATED, state_);
  state_ = IsAttached() ? WORKER_PAUSED_FOR_REATTACH : WORKER_UNINSPECTED;
  worker_id_ = worker_id;
  WorkerCreated();
}

void EmbeddedWorkerDevToolsAgentHost::WorkerDestroyed() {
  DCHECK_NE(WORKER_TERMINATED, state_);
  if (state_ == WORKER_INSPECTED) {
    DCHECK(IsAttached());
    // Client host is debugging this worker agent host.
    base::Callback<void(const std::string&)> raw_message_callback(
        base::Bind(&EmbeddedWorkerDevToolsAgentHost::SendMessageToClient,
                base::Unretained(this)));
    devtools::worker::Client worker(raw_message_callback);
    worker.DisconnectedFromWorker(
        devtools::worker::DisconnectedFromWorkerParams::Create());
    DetachFromWorker();
  }
  state_ = WORKER_TERMINATED;
  Release();  // Balanced in WorkerCreated().
}

bool EmbeddedWorkerDevToolsAgentHost::IsTerminated() {
  return state_ == WORKER_TERMINATED;
}

bool EmbeddedWorkerDevToolsAgentHost::Matches(
    const SharedWorkerInstance& other) {
  return false;
}

bool EmbeddedWorkerDevToolsAgentHost::Matches(
    const ServiceWorkerIdentifier& other) {
  return false;
}

EmbeddedWorkerDevToolsAgentHost::EmbeddedWorkerDevToolsAgentHost(
    WorkerId worker_id)
    : state_(WORKER_UNINSPECTED),
      worker_id_(worker_id) {
  WorkerCreated();
}

EmbeddedWorkerDevToolsAgentHost::~EmbeddedWorkerDevToolsAgentHost() {
  DCHECK_EQ(WORKER_TERMINATED, state_);
  EmbeddedWorkerDevToolsManager::GetInstance()->RemoveInspectedWorkerData(
      worker_id_);
}

void EmbeddedWorkerDevToolsAgentHost::AttachToWorker() {
  if (RenderProcessHost* host = RenderProcessHost::FromID(worker_id_.first))
    host->AddRoute(worker_id_.second, this);
}

void EmbeddedWorkerDevToolsAgentHost::DetachFromWorker() {
  if (RenderProcessHost* host = RenderProcessHost::FromID(worker_id_.first))
    host->RemoveRoute(worker_id_.second);
}

void EmbeddedWorkerDevToolsAgentHost::WorkerCreated() {
  AddRef();  // Balanced in WorkerDestroyed()
}

void EmbeddedWorkerDevToolsAgentHost::OnDispatchOnInspectorFrontend(
    const std::string& message,
    uint32 total_size) {
  if (!IsAttached())
    return;

  ProcessChunkedMessageFromAgent(message, total_size);
}

void EmbeddedWorkerDevToolsAgentHost::OnSaveAgentRuntimeState(
    const std::string& state) {
  saved_agent_state_ = state;
}

}  // namespace content
