// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/websharedworker_impl.h"

#include "chrome/common/render_messages.h"
#include "chrome/common/webmessageportchannel_impl.h"
#include "chrome/common/worker_messages.h"
#include "webkit/api/public/WebURL.h"

WebSharedWorkerImpl::WebSharedWorkerImpl(const GURL& url,
                                         const string16& name,
                                         ChildThread* child_thread,
                                         int route_id,
                                         int render_view_route_id)
    : WebWorkerBase(child_thread, route_id, render_view_route_id),
      url_(url),
      name_(name) {
}

bool WebSharedWorkerImpl::isStarted() {
  return IsStarted();
}

void WebSharedWorkerImpl::startWorkerContext(
    const WebKit::WebURL& script_url,
    const WebKit::WebString& user_agent,
    const WebKit::WebString& source_code) {
  DCHECK(url_ == script_url);
  IPC::Message* create_message = new ViewHostMsg_CreateSharedWorker(
      url_, name_, render_view_route_id_, &route_id_);
  CreateWorkerContext(create_message, script_url, user_agent, source_code);
}

void WebSharedWorkerImpl::connect(WebKit::WebMessagePortChannel* channel) {
  WebMessagePortChannelImpl* webchannel =
        static_cast<WebMessagePortChannelImpl*>(channel);

  int message_port_id = webchannel->message_port_id();
  DCHECK(message_port_id != MSG_ROUTING_NONE);
  webchannel->QueueMessages();

  Send(new WorkerMsg_Connect(route_id_, message_port_id, MSG_ROUTING_NONE));
}

void WebSharedWorkerImpl::OnMessageReceived(const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(WebSharedWorkerImpl, message)
    IPC_MESSAGE_HANDLER(ViewMsg_WorkerCreated, OnWorkerCreated)
  IPC_END_MESSAGE_MAP()
}

void WebSharedWorkerImpl::OnWorkerCreated() {
  // The worker is created - now send off the CreateWorkerContext message and
  // any other queued messages
  SendQueuedMessages();
}

