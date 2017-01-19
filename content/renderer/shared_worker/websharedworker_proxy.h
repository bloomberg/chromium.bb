// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SHARED_WORKER_WEBSHAREDWORKER_PROXY_H_
#define CONTENT_RENDERER_SHARED_WORKER_WEBSHAREDWORKER_PROXY_H_

#include "base/macros.h"
#include "ipc/ipc_listener.h"
#include "third_party/WebKit/public/web/WebSharedWorkerConnectListener.h"
#include "third_party/WebKit/public/web/WebSharedWorkerCreationErrors.h"

struct ViewHostMsg_CreateWorker_Params;

namespace blink {
class WebMessagePortChannel;
}

namespace IPC {
class MessageRouter;
}

namespace content {

// Implementation of the WebSharedWorker APIs. This object is intended to only
// live long enough to allow the caller to send a "connect" event to the worker
// thread. Once the connect event has been sent, all future communication will
// happen via the WebMessagePortChannel. This instance will self-destruct when a
// connection is established.
class WebSharedWorkerProxy : private IPC::Listener {
 public:
  // |channel| should be passed with its ownership.
  WebSharedWorkerProxy(
      std::unique_ptr<blink::WebSharedWorkerConnectListener> listener,
      ViewHostMsg_CreateWorker_Params params,
      blink::WebMessagePortChannel* channel);
  ~WebSharedWorkerProxy() override;

 private:
  void connect(ViewHostMsg_CreateWorker_Params params,
               blink::WebMessagePortChannel* channel);

  // IPC::Listener implementation.
  bool OnMessageReceived(const IPC::Message& message) override;

  void OnWorkerCreated();
  void OnWorkerScriptLoadFailed();
  void OnWorkerConnected();

  // Routing id associated with this worker - used to receive messages from the
  // worker, and also to route messages to the worker (WorkerService contains
  // a map that maps between these renderer-side route IDs and worker-side
  // routing ids).
  int route_id_;

  IPC::MessageRouter* const router_;

  int message_port_id_;
  std::unique_ptr<blink::WebSharedWorkerConnectListener> listener_;

  DISALLOW_COPY_AND_ASSIGN(WebSharedWorkerProxy);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SHARED_WORKER_WEBSHAREDWORKER_PROXY_H_
