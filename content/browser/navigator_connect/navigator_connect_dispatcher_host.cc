// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/navigator_connect/navigator_connect_dispatcher_host.h"

#include "content/browser/message_port_service.h"
#include "content/browser/navigator_connect/navigator_connect_context_impl.h"
#include "content/browser/service_worker/service_worker_context_wrapper.h"
#include "content/common/navigator_connect_messages.h"
#include "content/public/common/navigator_connect_client.h"

namespace content {

NavigatorConnectDispatcherHost::NavigatorConnectDispatcherHost(
    const scoped_refptr<NavigatorConnectContextImpl>& navigator_connect_context,
    MessagePortMessageFilter* message_port_message_filter)
    : BrowserMessageFilter(NavigatorConnectMsgStart),
      navigator_connect_context_(navigator_connect_context),
      message_port_message_filter_(message_port_message_filter) {
}

NavigatorConnectDispatcherHost::~NavigatorConnectDispatcherHost() {
}

bool NavigatorConnectDispatcherHost::OnMessageReceived(
    const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(NavigatorConnectDispatcherHost, message)
    IPC_MESSAGE_HANDLER(NavigatorConnectHostMsg_Connect, OnConnect)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

void NavigatorConnectDispatcherHost::OnConnect(
    int thread_id,
    int request_id,
    const NavigatorConnectClient& client) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  DCHECK(client.message_port_id == MSG_ROUTING_NONE)
      << "Connect request should not include a message port";

  navigator_connect_context_->Connect(
      client, message_port_message_filter_,
      base::Bind(&NavigatorConnectDispatcherHost::OnConnectResult, this,
                 thread_id, request_id));
}

void NavigatorConnectDispatcherHost::OnConnectResult(
    int thread_id,
    int request_id,
    const TransferredMessagePort& message_port,
    int message_port_route_id,
    bool accept_connection) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  Send(new NavigatorConnectMsg_ConnectResult(
      thread_id, request_id, message_port, message_port_route_id,
      accept_connection));
}

}  // namespace content
