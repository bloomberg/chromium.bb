// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_WORKER_DEVTOOLS_AGENT_PROXY_H_
#define CONTENT_RENDERER_WORKER_DEVTOOLS_AGENT_PROXY_H_
#pragma once

#include <string>

#include "base/basictypes.h"

namespace IPC {
class Message;
}

namespace WebKit {
class WebWorkerClient;
}

class WebWorkerBase;

class WorkerDevToolsAgentProxy {
 public:
  WorkerDevToolsAgentProxy(WebWorkerBase*,
                           int route_id,
                           WebKit::WebWorkerClient*);
  ~WorkerDevToolsAgentProxy();

  void WorkerProxyDestroyed();
  void SetRouteId(int route_id);
  bool OnMessageReceived(const IPC::Message& message);

  void AttachDevTools();
  void DetachDevTools();
  void SendDevToolsMessage(const std::string&);

 private:
  void OnDispatchMessageFromWorker(const std::string& message);

  void Send(IPC::Message* message);

  int route_id_;
  WebWorkerBase* webworker_;
  WebKit::WebWorkerClient* webworker_client_;

  DISALLOW_COPY_AND_ASSIGN(WorkerDevToolsAgentProxy);
};

#endif  // CONTENT_RENDERER_WORKER_DEVTOOLS_AGENT_PROXY_H_
