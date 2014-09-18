// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/devtools/embedded_worker_devtools_agent_host.h"

#include "base/strings/utf_string_conversions.h"
#include "content/browser/devtools/devtools_protocol.h"
#include "content/browser/devtools/devtools_protocol_constants.h"
#include "content/browser/service_worker/service_worker_context_core.h"
#include "content/browser/service_worker/service_worker_version.h"
#include "content/browser/shared_worker/shared_worker_service_impl.h"
#include "content/common/devtools_messages.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_process_host.h"

namespace content {

namespace {

void TerminateSharedWorkerOnIO(
    EmbeddedWorkerDevToolsAgentHost::WorkerId worker_id) {
  SharedWorkerServiceImpl::GetInstance()->TerminateWorker(
      worker_id.first, worker_id.second);
}

void StatusNoOp(ServiceWorkerStatusCode status) {
}

void TerminateServiceWorkerOnIO(
    base::WeakPtr<ServiceWorkerContextCore> context_weak,
    int64 version_id) {
  if (ServiceWorkerContextCore* context = context_weak.get()) {
    if (ServiceWorkerVersion* version = context->GetLiveVersion(version_id))
      version->StopWorker(base::Bind(&StatusNoOp));
  }
}

}

EmbeddedWorkerDevToolsAgentHost::EmbeddedWorkerDevToolsAgentHost(
    WorkerId worker_id,
    const SharedWorkerInstance& shared_worker)
    : shared_worker_(new SharedWorkerInstance(shared_worker)),
      state_(WORKER_UNINSPECTED),
      worker_id_(worker_id) {
  WorkerCreated();
}

EmbeddedWorkerDevToolsAgentHost::EmbeddedWorkerDevToolsAgentHost(
    WorkerId worker_id,
    const ServiceWorkerIdentifier& service_worker,
    bool debug_service_worker_on_start)
    : service_worker_(new ServiceWorkerIdentifier(service_worker)),
      state_(WORKER_UNINSPECTED),
      worker_id_(worker_id) {
  if (debug_service_worker_on_start)
    state_ = WORKER_PAUSED_FOR_DEBUG_ON_START;
  WorkerCreated();
}

bool EmbeddedWorkerDevToolsAgentHost::IsWorker() const {
  return true;
}

DevToolsAgentHost::Type EmbeddedWorkerDevToolsAgentHost::GetType() {
  return shared_worker_ ? TYPE_SHARED_WORKER : TYPE_SERVICE_WORKER;
}

std::string EmbeddedWorkerDevToolsAgentHost::GetTitle() {
  if (shared_worker_ && shared_worker_->name().length())
    return base::UTF16ToUTF8(shared_worker_->name());
  if (RenderProcessHost* host = RenderProcessHost::FromID(worker_id_.first)) {
    return base::StringPrintf("Worker pid:%d",
                              base::GetProcId(host->GetHandle()));
  }
  return "";
}

GURL EmbeddedWorkerDevToolsAgentHost::GetURL() {
  if (shared_worker_)
    return shared_worker_->url();
  if (service_worker_)
    return service_worker_->url();
  return GURL();
}

bool EmbeddedWorkerDevToolsAgentHost::Activate() {
  return false;
}

bool EmbeddedWorkerDevToolsAgentHost::Close() {
  if (shared_worker_) {
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
        base::Bind(&TerminateSharedWorkerOnIO, worker_id_));
    return true;
  }
  if (service_worker_) {
    BrowserThread::PostTask(BrowserThread::IO, FROM_HERE,
        base::Bind(&TerminateServiceWorkerOnIO,
                   service_worker_->context_weak(),
                   service_worker_->version_id()));
    return true;
  }
  return false;
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

void EmbeddedWorkerDevToolsAgentHost::OnClientDetached() {
  if (state_ == WORKER_INSPECTED) {
    state_ = WORKER_UNINSPECTED;
    DetachFromWorker();
  } else if (state_ == WORKER_PAUSED_FOR_REATTACH) {
    state_ = WORKER_UNINSPECTED;
  }
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

void EmbeddedWorkerDevToolsAgentHost::WorkerContextStarted() {
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
    std::string notification =
        DevToolsProtocol::CreateNotification(
            devtools::Worker::disconnectedFromWorker::kName, NULL)->Serialize();
    SendMessageToClient(notification);
    DetachFromWorker();
  }
  state_ = WORKER_TERMINATED;
  Release();  // Balanced in WorkerCreated()
}

bool EmbeddedWorkerDevToolsAgentHost::Matches(
    const SharedWorkerInstance& other) {
  return shared_worker_ && shared_worker_->Matches(other);
}

bool EmbeddedWorkerDevToolsAgentHost::Matches(
    const ServiceWorkerIdentifier& other) {
  return service_worker_ && service_worker_->Matches(other);
}

bool EmbeddedWorkerDevToolsAgentHost::IsTerminated() {
  return state_ == WORKER_TERMINATED;
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
    const std::string& message) {
  SendMessageToClient(message);
}

void EmbeddedWorkerDevToolsAgentHost::OnSaveAgentRuntimeState(
    const std::string& state) {
  saved_agent_state_ = state;
}

}  // namespace content
