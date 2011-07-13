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
class WebSharedWorker;
class WebString;
class WebWorker;
}

class WorkerDevToolsAgent {
 public:
  static WorkerDevToolsAgent* CreateForDedicatedWorker(
      int route_id,
      WebKit::WebWorker*);
  static WorkerDevToolsAgent* CreateForSharedWorker(
      int route_id,
      WebKit::WebSharedWorker*);
  virtual ~WorkerDevToolsAgent();

  // Called on the Worker thread.
  virtual bool OnMessageReceived(const IPC::Message& message) = 0;
  virtual void SendDevToolsMessage(const WebKit::WebString&) = 0;

 protected:
  explicit WorkerDevToolsAgent(int route_id);

  bool Send(IPC::Message* message);
  const int route_id_;

  DISALLOW_COPY_AND_ASSIGN(WorkerDevToolsAgent);
};

#endif  // CONTENT_WORKER_WORKER_DEVTOOLS_AGENT_H_
