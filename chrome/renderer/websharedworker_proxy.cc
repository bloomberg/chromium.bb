// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/websharedworker_proxy.h"

#include "chrome/common/render_messages.h"
#include "chrome/common/webmessageportchannel_impl.h"
#include "chrome/common/worker_messages.h"
#include "third_party/WebKit/WebKit/chromium/public/WebURL.h"

WebSharedWorkerProxy::WebSharedWorkerProxy(ChildThread* child_thread,
                                           int route_id,
                                           int render_view_route_id)
    : WebWorkerBase(child_thread, route_id, render_view_route_id),
      m_connectListener(NULL) {
}

bool WebSharedWorkerProxy::isStarted() {
  return IsStarted();
}

void WebSharedWorkerProxy::startWorkerContext(
    const WebKit::WebURL& script_url,
    const WebKit::WebString& name,
    const WebKit::WebString& user_agent,
    const WebKit::WebString& source_code) {
  CreateWorkerContext(script_url, true, name, user_agent, source_code);
}

void WebSharedWorkerProxy::terminateWorkerContext() {
  // This API should only be invoked from worker context.
  NOTREACHED();
}

void WebSharedWorkerProxy::clientDestroyed() {
  // This API should only be invoked from worker context.
  NOTREACHED();
}

void WebSharedWorkerProxy::connect(WebKit::WebMessagePortChannel* channel,
                                   ConnectListener* listener) {
  WebMessagePortChannelImpl* webchannel =
        static_cast<WebMessagePortChannelImpl*>(channel);

  int message_port_id = webchannel->message_port_id();
  DCHECK(message_port_id != MSG_ROUTING_NONE);
  webchannel->QueueMessages();

  Send(new WorkerMsg_Connect(route_id_, message_port_id, MSG_ROUTING_NONE));
  if (HasQueuedMessages()) {
    m_connectListener = listener;
  } else {
    listener->connected();
    // The listener may free this object, so do not access the object after
    // this point.
  }
}

void WebSharedWorkerProxy::OnMessageReceived(const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(WebSharedWorkerProxy, message)
    IPC_MESSAGE_HANDLER(ViewMsg_WorkerCreated, OnWorkerCreated)
  IPC_END_MESSAGE_MAP()
}

void WebSharedWorkerProxy::OnWorkerCreated() {
  // The worker is created - now send off the CreateWorkerContext message and
  // any other queued messages
  SendQueuedMessages();

  // Inform any listener that the pending connect event has been sent
  // (this can result in this object being freed).
  if (m_connectListener) {
    m_connectListener->connected();
  }
}

