// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/websharedworker_proxy.h"

#include <stddef.h>

#include "content/child/webmessageportchannel_impl.h"
#include "content/common/view_messages.h"
#include "content/common/worker_messages.h"
#include "ipc/message_router.h"

namespace content {

WebSharedWorkerProxy::WebSharedWorkerProxy(IPC::MessageRouter* router,
                                           int route_id)
    : route_id_(route_id),
      router_(router),
      message_port_id_(MSG_ROUTING_NONE),
      connect_listener_(nullptr) {
  DCHECK_NE(MSG_ROUTING_NONE, route_id_);
  router_->AddRoute(route_id_, this);
}

WebSharedWorkerProxy::~WebSharedWorkerProxy() {
  router_->RemoveRoute(route_id_);
}

void WebSharedWorkerProxy::connect(blink::WebMessagePortChannel* channel,
                                   ConnectListener* listener) {
  DCHECK_EQ(MSG_ROUTING_NONE, message_port_id_);
  WebMessagePortChannelImpl* webchannel =
        static_cast<WebMessagePortChannelImpl*>(channel);
  message_port_id_ = webchannel->message_port_id();
  DCHECK_NE(MSG_ROUTING_NONE, message_port_id_);
  webchannel->QueueMessages();
  connect_listener_ = listener;
  // An actual connection request will be issued on OnWorkerCreated().
}

bool WebSharedWorkerProxy::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(WebSharedWorkerProxy, message)
    IPC_MESSAGE_HANDLER(ViewMsg_WorkerCreated, OnWorkerCreated)
    IPC_MESSAGE_HANDLER(ViewMsg_WorkerScriptLoadFailed,
                        OnWorkerScriptLoadFailed)
    IPC_MESSAGE_HANDLER(ViewMsg_WorkerConnected,
                        OnWorkerConnected)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void WebSharedWorkerProxy::OnWorkerCreated() {
  // connect() should be called before.
  DCHECK_NE(MSG_ROUTING_NONE, message_port_id_);

  // The worker is created - now send off the connection request.
  router_->Send(
      new ViewHostMsg_ConnectToWorker(route_id_, message_port_id_));
}

void WebSharedWorkerProxy::OnWorkerScriptLoadFailed() {
  if (connect_listener_) {
    // This can result in this object being freed.
    connect_listener_->scriptLoadFailed();
  }
}

void WebSharedWorkerProxy::OnWorkerConnected() {
  if (connect_listener_) {
    // This can result in this object being freed.
    connect_listener_->connected();
  }
}

}  // namespace content
