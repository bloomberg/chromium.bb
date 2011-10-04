// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/worker_devtools_agent_proxy.h"

#include "content/common/devtools_messages.h"
#include "content/renderer/webworker_base.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebWorkerClient.h"

using WebKit::WebString;
using WebKit::WebWorkerClient;

WorkerDevToolsAgentProxy::WorkerDevToolsAgentProxy(WebWorkerBase* webworker,
                                                   int route_id,
                                                   WebWorkerClient* client)
    : route_id_(route_id),
      webworker_(webworker),
      webworker_client_(client) {
}

WorkerDevToolsAgentProxy::~WorkerDevToolsAgentProxy() {
}

void WorkerDevToolsAgentProxy::WorkerProxyDestroyed() {
  delete this;
}

void WorkerDevToolsAgentProxy::SetRouteId(int route_id) {
  route_id_ = route_id;
}

// Called on the Renderer thread.
bool WorkerDevToolsAgentProxy::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WorkerDevToolsAgentProxy, message)
    IPC_MESSAGE_HANDLER(DevToolsAgentMsg_DispatchMessageFromWorker,
                        OnDispatchMessageFromWorker)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void WorkerDevToolsAgentProxy::AttachDevTools() {
  Send(new DevToolsAgentMsg_Attach(route_id_, DevToolsRuntimeProperties()));
}

void WorkerDevToolsAgentProxy::DetachDevTools() {
  Send(new DevToolsAgentMsg_Detach(route_id_));
}

void WorkerDevToolsAgentProxy::SendDevToolsMessage(
    const std::string& message) {
  Send(new DevToolsAgentMsg_DispatchOnInspectorBackend(route_id_, message));
}

void WorkerDevToolsAgentProxy::OnDispatchMessageFromWorker(
    const std::string& message) {
  webworker_client_->dispatchDevToolsMessage(WebString::fromUTF8(message));
}

void WorkerDevToolsAgentProxy::Send(IPC::Message* message) {
  webworker_->Send(message);
}
