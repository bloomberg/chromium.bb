// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SHARED_WORKER_WEBSHAREDWORKER_PROXY_H_
#define CONTENT_RENDERER_SHARED_WORKER_WEBSHAREDWORKER_PROXY_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "ipc/ipc_listener.h"
#include "third_party/WebKit/public/web/WebSharedWorkerConnector.h"

namespace IPC {
class MessageRouter;
}

namespace content {

// Implementation of the WebSharedWorker APIs. This object is intended to only
// live long enough to allow the caller to send a "connect" event to the worker
// thread. Once the connect event has been sent, all future communication will
// happen via the WebMessagePortChannel, and the WebSharedWorker instance will
// be freed.
class WebSharedWorkerProxy : public blink::WebSharedWorkerConnector,
                             private IPC::Listener {
 public:
  WebSharedWorkerProxy(IPC::MessageRouter* router, int route_id);
  ~WebSharedWorkerProxy() override;

  // Implementations of WebSharedWorkerConnector APIs
  void connect(blink::WebMessagePortChannel* channel,
               ConnectListener* listener) override;

 private:
  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& message) override;

  void OnWorkerCreated();
  void OnWorkerScriptLoadFailed();
  void OnWorkerConnected();

  // Routing id associated with this worker - used to receive messages from the
  // worker, and also to route messages to the worker (WorkerService contains
  // a map that maps between these renderer-side route IDs and worker-side
  // routing ids).
  const int route_id_;

  IPC::MessageRouter* const router_;

  int message_port_id_;
  ConnectListener* connect_listener_;

  DISALLOW_COPY_AND_ASSIGN(WebSharedWorkerProxy);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SHARED_WORKER_WEBSHAREDWORKER_PROXY_H_
