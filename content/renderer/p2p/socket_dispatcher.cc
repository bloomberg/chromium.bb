// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/p2p/socket_dispatcher.h"

#include "base/message_loop_proxy.h"
#include "content/common/p2p_messages.h"

P2PSocketDispatcher::P2PSocketDispatcher(RenderView* render_view)
    : RenderViewObserver(render_view),
      message_loop_(base::MessageLoopProxy::CreateForCurrentThread()) {
}

P2PSocketDispatcher::~P2PSocketDispatcher() {
  for (IDMap<P2PSocketClient>::iterator i(&clients_); !i.IsAtEnd();
       i.Advance()) {
    i.GetCurrentValue()->Detach();
  }
}

bool P2PSocketDispatcher::OnMessageReceived(const IPC::Message& message) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP(P2PSocketDispatcher, message)
    IPC_MESSAGE_HANDLER(P2PMsg_OnSocketCreated, OnSocketCreated)
    IPC_MESSAGE_HANDLER(P2PMsg_OnError, OnError)
    IPC_MESSAGE_HANDLER(P2PMsg_OnDataReceived, OnDataReceived)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP()
  return handled;
}

int P2PSocketDispatcher::RegisterClient(P2PSocketClient* client) {
  return clients_.Add(client);
}

void P2PSocketDispatcher::UnregisterClient(int id) {
  clients_.Remove(id);
}

void P2PSocketDispatcher::SendP2PMessage(IPC::Message* msg) {
  msg->set_routing_id(routing_id());
  Send(msg);
}

base::MessageLoopProxy* P2PSocketDispatcher::message_loop() {
  return message_loop_;
}

void P2PSocketDispatcher::OnSocketCreated(
    int socket_id, const net::IPEndPoint& address) {
  P2PSocketClient* client = GetClient(socket_id);
  if (client) {
    client->OnSocketCreated(address);
  }
}

void P2PSocketDispatcher::OnError(int socket_id) {
  P2PSocketClient* client = GetClient(socket_id);
  if (client) {
    client->OnError();
  }
}

void P2PSocketDispatcher::OnDataReceived(
    int socket_id, const net::IPEndPoint& address,
    const std::vector<char>& data) {
  P2PSocketClient* client = GetClient(socket_id);
  if (client) {
    client->OnDataReceived(address, data);
  }
}

P2PSocketClient* P2PSocketDispatcher::GetClient(int socket_id) {
  P2PSocketClient* client = clients_.Lookup(socket_id);
  if (client == NULL) {
    // This may happen if the socket was closed, but the browser side
    // hasn't processed the close message by the time it sends the
    // message to the renderer.
    VLOG(1) << "Received P2P message for socket that doesn't exist.";
    return NULL;
  }

  return client;
}
