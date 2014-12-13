// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/navigator_connect/navigator_connect_dispatcher_host.h"

#include "content/browser/message_port_service.h"
#include "content/common/navigator_connect_messages.h"
#include "content/common/navigator_connect_types.h"

namespace content {

NavigatorConnectDispatcherHost::NavigatorConnectDispatcherHost()
    : BrowserMessageFilter(NavigatorConnectMsgStart) {
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
    const CrossOriginServiceWorkerClient& client) {
  // TODO(mek): Actually setup a connection.

  // Close port since connection fails.
  MessagePortService::GetInstance()->ClosePort(client.message_port_id);
  Send(new NavigatorConnectMsg_ConnectResult(thread_id, request_id, false));
}

}  // namespace content
