// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/shared_worker/websharedworker_proxy.h"

#include <stddef.h>

#include "content/child/child_thread_impl.h"
#include "content/child/webmessageportchannel_impl.h"
#include "content/common/view_messages.h"
#include "content/common/worker_messages.h"
#include "ipc/message_router.h"

namespace content {

WebSharedWorkerProxy::WebSharedWorkerProxy(
    std::unique_ptr<blink::WebSharedWorkerConnectListener> listener,
    ViewHostMsg_CreateWorker_Params params,
    blink::WebMessagePortChannel* channel)
    : route_id_(MSG_ROUTING_NONE),
      router_(ChildThreadImpl::current()->GetRouter()),
      message_port_id_(MSG_ROUTING_NONE),
      listener_(std::move(listener)) {
  connect(params, channel);
}

WebSharedWorkerProxy::~WebSharedWorkerProxy() {
  DCHECK_NE(MSG_ROUTING_NONE, route_id_);
  router_->RemoveRoute(route_id_);
}

void WebSharedWorkerProxy::connect(ViewHostMsg_CreateWorker_Params params,
                                   blink::WebMessagePortChannel* channel) {
  // Send synchronous IPC to get |route_id|.
  // TODO(nhiroki): Stop using synchronous IPC (https://crbug.com/679654).
  ViewHostMsg_CreateWorker_Reply reply;
  router_->Send(new ViewHostMsg_CreateWorker(params, &reply));
  route_id_ = reply.route_id;
  router_->AddRoute(route_id_, this);
  listener_->workerCreated(reply.error);

  DCHECK_EQ(MSG_ROUTING_NONE, message_port_id_);
  WebMessagePortChannelImpl* webchannel =
        static_cast<WebMessagePortChannelImpl*>(channel);
  message_port_id_ = webchannel->message_port_id();
  DCHECK_NE(MSG_ROUTING_NONE, message_port_id_);
  webchannel->QueueMessages();
  // |webchannel| is intentionally leaked here: it'll be removed at
  // WebMessagePortChannelImpl::OnMessagesQueued().

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
  // The worker is created - now send off the connection request.
  router_->Send(
      new ViewHostMsg_ConnectToWorker(route_id_, message_port_id_));
}

void WebSharedWorkerProxy::OnWorkerScriptLoadFailed() {
  listener_->scriptLoadFailed();
  delete this;
}

void WebSharedWorkerProxy::OnWorkerConnected() {
  listener_->connected();
  delete this;
}

}  // namespace content
