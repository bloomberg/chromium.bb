// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_SHARED_WORKER_WEBSHAREDWORKER_PROXY_H_
#define CONTENT_RENDERER_SHARED_WORKER_WEBSHAREDWORKER_PROXY_H_

#include <set>

#include "base/macros.h"
#include "content/common/message_port.h"
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

// A proxy between a renderer process hosting a document and the browser
// process. This instance has the same lifetime as a shared worker, and
// self-destructs when the worker is destroyed.
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
  void OnWorkerConnected(const std::set<uint32_t>& used_features);
  void OnWorkerDestroyed();
  void OnCountFeature(uint32_t feature);

  // Routing id associated with this worker - used to receive messages from the
  // worker, and also to route messages to the worker (WorkerService contains
  // a map that maps between these renderer-side route IDs and worker-side
  // routing ids).
  int route_id_;

  IPC::MessageRouter* const router_;

  MessagePort message_port_;
  std::unique_ptr<blink::WebSharedWorkerConnectListener> listener_;

  DISALLOW_COPY_AND_ASSIGN(WebSharedWorkerProxy);
};

}  // namespace content

#endif  // CONTENT_RENDERER_SHARED_WORKER_WEBSHAREDWORKER_PROXY_H_
