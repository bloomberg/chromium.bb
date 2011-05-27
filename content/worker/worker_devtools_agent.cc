// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/worker/worker_devtools_agent.h"

#include "content/common/devtools_messages.h"
#include "content/worker/worker_thread.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebWorker.h"

using WebKit::WebString;
using WebKit::WebWorker;

WorkerDevToolsAgent::WorkerDevToolsAgent(int route_id, WebWorker* webworker)
    : route_id_(route_id),
      webworker_(webworker) {
}

WorkerDevToolsAgent::~WorkerDevToolsAgent() {
}

// Called on the Worker thread.
bool WorkerDevToolsAgent::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WorkerDevToolsAgent, message)
    IPC_MESSAGE_HANDLER(WorkerDevToolsAgentMsg_Attach, OnAttach)
    IPC_MESSAGE_HANDLER(WorkerDevToolsAgentMsg_Detach, OnDetach)
    IPC_MESSAGE_HANDLER(WorkerDevToolsAgentMsg_DispatchOnInspectorBackend,
                        OnDispatchOnInspectorBackend)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void WorkerDevToolsAgent::OnAttach() {
  webworker_->attachDevTools();
}

void WorkerDevToolsAgent::OnDetach() {
  webworker_->detachDevTools();
}

void WorkerDevToolsAgent::OnDispatchOnInspectorBackend(
    const std::string& message) {
  webworker_->dispatchDevToolsMessage(WebString::fromUTF8(message));
}

bool WorkerDevToolsAgent::Send(IPC::Message* message) {
  return WorkerThread::current()->Send(message);
}

void WorkerDevToolsAgent::SendDevToolsMessage(const WebString& message) {
  Send(new DevToolsAgentMsg_DispatchMessageFromWorker(route_id_,
                                                      message.utf8()));
}
