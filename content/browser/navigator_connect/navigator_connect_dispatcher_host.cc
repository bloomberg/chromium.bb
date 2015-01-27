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
    const scoped_refptr<NavigatorConnectContextImpl>& navigator_connect_context)
    : BrowserMessageFilter(NavigatorConnectMsgStart),
      navigator_connect_context_(navigator_connect_context) {
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

  navigator_connect_context_->Connect(
      client, base::Bind(&NavigatorConnectDispatcherHost::OnConnectResult, this,
                         thread_id, request_id));
}

void NavigatorConnectDispatcherHost::OnConnectResult(
    int thread_id,
    int request_id,
    bool accept_connection) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  Send(new NavigatorConnectMsg_ConnectResult(thread_id, request_id,
                                             accept_connection));
}

}  // namespace content
