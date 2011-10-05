// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/worker/worker_devtools_agent.h"

#include "content/common/devtools_messages.h"
#include "content/worker/worker_thread.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebCString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebSharedWorker.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebWorker.h"

using WebKit::WebSharedWorker;
using WebKit::WebString;
using WebKit::WebWorker;

namespace {

template<class T>
class WorkerDevToolsAgentImpl : public WorkerDevToolsAgent {
 public:
  WorkerDevToolsAgentImpl(int route_id, T* webworker)
      : WorkerDevToolsAgent(route_id),
        webworker_(webworker) {}

 private:
  virtual ~WorkerDevToolsAgentImpl() {}

  // Called on the Worker thread.
  virtual bool OnMessageReceived(const IPC::Message& message) {
    bool handled = true;
    IPC_BEGIN_MESSAGE_MAP(WorkerDevToolsAgentImpl, message)
      IPC_MESSAGE_HANDLER(DevToolsAgentMsg_Attach, OnAttach)
      IPC_MESSAGE_HANDLER(DevToolsAgentMsg_Reattach, OnReattach)
      IPC_MESSAGE_HANDLER(DevToolsAgentMsg_Detach, OnDetach)
      IPC_MESSAGE_HANDLER(DevToolsAgentMsg_DispatchOnInspectorBackend,
                          OnDispatchOnInspectorBackend)
      IPC_MESSAGE_UNHANDLED(handled = false)
    IPC_END_MESSAGE_MAP()
    return handled;
  }

  virtual void SendDevToolsMessage(const WebKit::WebString& message);

  void OnAttach() {
    webworker_->attachDevTools();
  }

  void OnReattach(const std::string&) {
    OnAttach();
  }

  void OnDetach() {
    webworker_->detachDevTools();
  }

  void OnDispatchOnInspectorBackend(
      const std::string& message) {
    webworker_->dispatchDevToolsMessage(WebString::fromUTF8(message));
  }

  T* webworker_;

  DISALLOW_COPY_AND_ASSIGN(WorkerDevToolsAgentImpl);
};


template<>
void WorkerDevToolsAgentImpl<WebWorker>::SendDevToolsMessage(
    const WebKit::WebString& message) {
  Send(new DevToolsAgentMsg_DispatchMessageFromWorker(route_id_,
                                                      message.utf8()));
}

template<>
void WorkerDevToolsAgentImpl<WebSharedWorker>::SendDevToolsMessage(
    const WebKit::WebString& message) {
    IPC::Message m = DevToolsClientMsg_DispatchOnInspectorFrontend(
        route_id_,
        message.utf8());
    Send(new DevToolsHostMsg_ForwardToClient(route_id_, m));
}

}  // namespace

WorkerDevToolsAgent::WorkerDevToolsAgent(int route_id)
    : route_id_(route_id) {
}

WorkerDevToolsAgent* WorkerDevToolsAgent::CreateForDedicatedWorker(
    int route_id,
    WebWorker* webworker) {
  return new WorkerDevToolsAgentImpl<WebWorker>(route_id, webworker);
}

WorkerDevToolsAgent* WorkerDevToolsAgent::CreateForSharedWorker(
    int route_id,
    WebSharedWorker* webshared_worker) {
  return new WorkerDevToolsAgentImpl<WebSharedWorker>(route_id,
                                                      webshared_worker);
}

WorkerDevToolsAgent::~WorkerDevToolsAgent() {
}

bool WorkerDevToolsAgent::Send(IPC::Message* message) {
  return WorkerThread::current()->Send(message);
}
