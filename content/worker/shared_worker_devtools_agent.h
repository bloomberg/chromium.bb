// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WORKER_SHARED_WORKER_DEVTOOLS_AGENT_H_
#define CONTENT_WORKER_SHARED_WORKER_DEVTOOLS_AGENT_H_

#include <string>

#include "base/basictypes.h"

namespace IPC {
class Message;
}

namespace blink {
class WebSharedWorker;
class WebString;
}

namespace content {

class SharedWorkerDevToolsAgent {
 public:
  SharedWorkerDevToolsAgent(int route_id, blink::WebSharedWorker*);
  ~SharedWorkerDevToolsAgent();

  // Called on the Worker thread.
  bool OnMessageReceived(const IPC::Message& message);
  void SendDevToolsMessage(const blink::WebString&);
  void SaveDevToolsAgentState(const blink::WebString& state);

 private:
  void OnAttach();
  void OnReattach(const std::string&);
  void OnDetach();
  void OnDispatchOnInspectorBackend(const std::string& message);
  void OnPauseWorkerContextOnStart();
  void OnResumeWorkerContext();

  bool Send(IPC::Message* message);
  const int route_id_;
  blink::WebSharedWorker* webworker_;

  DISALLOW_COPY_AND_ASSIGN(SharedWorkerDevToolsAgent);
};

}  // namespace content

#endif  // CONTENT_WORKER_SHARED_WORKER_DEVTOOLS_AGENT_H_
