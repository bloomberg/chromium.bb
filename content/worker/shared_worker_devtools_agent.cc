// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/worker/shared_worker_devtools_agent.h"

#include "content/common/devtools_messages.h"
#include "content/worker/worker_thread.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebCString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSharedWorker.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/platform/WebString.h"

using WebKit::WebSharedWorker;
using WebKit::WebString;

SharedWorkerDevToolsAgent::SharedWorkerDevToolsAgent(
    int route_id,
    WebSharedWorker* webworker)
    : route_id_(route_id),
      webworker_(webworker) {
}

SharedWorkerDevToolsAgent::~SharedWorkerDevToolsAgent() {
}

// Called on the Worker thread.
bool SharedWorkerDevToolsAgent::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(SharedWorkerDevToolsAgent, message)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_Attach, OnAttach)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_Reattach, OnReattach)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_Detach, OnDetach)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_DispatchOnInspectorBackend,
                        OnDispatchOnInspectorBackend)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_PauseWorkerContextOnStart,
                        OnPauseWorkerContextOnStart)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_ResumeWorkerContext,
                        OnResumeWorkerContext)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void SharedWorkerDevToolsAgent::SendDevToolsMessage(
    const WebKit::WebString& message) {
    Send(new DevToolsClientMsg_DispatchOnInspectorFrontend(
        route_id_,
        message.utf8()));
}

void SharedWorkerDevToolsAgent::SaveDevToolsAgentState(
    const WebKit::WebString& state) {
  Send(new DevToolsHostMsg_SaveAgentRuntimeState(route_id_,
                                                 state.utf8()));
}

void SharedWorkerDevToolsAgent::OnAttach() {
  webworker_->attachDevTools();
}

void SharedWorkerDevToolsAgent::OnReattach(const std::string& state) {
  webworker_->reattachDevTools(WebString::fromUTF8(state));
}

void SharedWorkerDevToolsAgent::OnDetach() {
  webworker_->detachDevTools();
}

void SharedWorkerDevToolsAgent::OnDispatchOnInspectorBackend(
    const std::string& message) {
  webworker_->dispatchDevToolsMessage(WebString::fromUTF8(message));
}

void SharedWorkerDevToolsAgent::OnPauseWorkerContextOnStart() {
  webworker_->pauseWorkerContextOnStart();
}

void SharedWorkerDevToolsAgent::OnResumeWorkerContext() {
  webworker_->resumeWorkerContext();
}

bool SharedWorkerDevToolsAgent::Send(IPC::Message* message) {
  return WorkerThread::current()->Send(message);
}
