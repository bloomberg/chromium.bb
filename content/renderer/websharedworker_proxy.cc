// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/websharedworker_proxy.h"

#include <stddef.h>

#include "content/child/webmessageportchannel_impl.h"
#include "content/common/view_messages.h"
#include "content/common/worker_messages.h"
#include "ipc/message_router.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/web/WebSharedWorkerClient.h"

namespace content {

WebSharedWorkerProxy::WebSharedWorkerProxy(IPC::MessageRouter* router,
                                           int route_id)
    : route_id_(route_id),
      router_(router),
      connect_listener_(nullptr),
      created_(false) {
  router_->AddRoute(route_id_, this);
}

WebSharedWorkerProxy::~WebSharedWorkerProxy() {
  Disconnect();
}

void WebSharedWorkerProxy::Disconnect() {
  if (route_id_ == MSG_ROUTING_NONE)
    return;

  // So the messages from WorkerContext (like WorkerContextDestroyed) do not
  // come after nobody is listening. Since Worker and WorkerContext can
  // terminate independently, already sent messages may still be in the pipe.
  router_->RemoveRoute(route_id_);

  route_id_ = MSG_ROUTING_NONE;
}

bool WebSharedWorkerProxy::Send(std::unique_ptr<IPC::Message> message) {
  // It's possible that messages will be sent before the worker is created, in
  // which case route_id_ will be none.  Or the worker object can be interacted
  // with before the browser process told us that it started, in which case we
  // also want to queue the message.
  if (!created_) {
    queued_messages_.push_back(std::move(message));
    return true;
  }

  // For now we proxy all messages to the worker process through the browser.
  // Revisit if we find this slow.
  // TODO(jabdelmalek): handle sync messages if we need them.
  return router_->Send(new ViewHostMsg_ForwardToWorker(*message));
}

void WebSharedWorkerProxy::SendQueuedMessages() {
  DCHECK(created_);
  DCHECK(queued_messages_.size());
  std::vector<std::unique_ptr<IPC::Message>> queued_messages;
  queued_messages.swap(queued_messages_);
  for (size_t i = 0; i < queued_messages.size(); ++i) {
    queued_messages[i]->set_routing_id(route_id_);
    Send(std::move(queued_messages[i]));
  }
}

void WebSharedWorkerProxy::connect(blink::WebMessagePortChannel* channel,
                                   ConnectListener* listener) {
  WebMessagePortChannelImpl* webchannel =
        static_cast<WebMessagePortChannelImpl*>(channel);

  int message_port_id = webchannel->message_port_id();
  DCHECK_NE(MSG_ROUTING_NONE, message_port_id);
  webchannel->QueueMessages();

  Send(base::MakeUnique<WorkerMsg_Connect>(route_id_, message_port_id,
                                           MSG_ROUTING_NONE));
  connect_listener_ = listener;
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
  created_ = true;
  // The worker is created - now send off the WorkerMsg_Connect message and
  // any other queued messages
  SendQueuedMessages();
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
