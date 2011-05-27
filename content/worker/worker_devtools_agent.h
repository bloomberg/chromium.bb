// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WORKER_WORKER_DEVTOOLS_AGENT_H_
#define CONTENT_WORKER_WORKER_DEVTOOLS_AGENT_H_
#pragma once

#include <string>

#include "base/basictypes.h"

namespace IPC {
class Message;
}

namespace WebKit {
class WebString;
class WebWorker;
}

class WorkerDevToolsAgent {
 public:
  WorkerDevToolsAgent(int route_id, WebKit::WebWorker*);
  ~WorkerDevToolsAgent();

  bool OnMessageReceived(const IPC::Message& message);

  void SendDevToolsMessage(const WebKit::WebString&);

 private:
  void OnAttach();
  void OnDetach();
  void OnDispatchOnInspectorBackend(const std::string& message);

  bool Send(IPC::Message* message);

  int route_id_;
  WebKit::WebWorker* webworker_;

  DISALLOW_COPY_AND_ASSIGN(WorkerDevToolsAgent);
};

#endif  // CONTENT_WORKER_WORKER_DEVTOOLS_AGENT_H_
