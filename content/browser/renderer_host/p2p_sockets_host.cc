// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/p2p_sockets_host.h"

#include "content/browser/renderer_host/p2p_socket_host.h"
#include "content/common/p2p_messages.h"

P2PSocketsHost::P2PSocketsHost() {
}

P2PSocketsHost::~P2PSocketsHost() {
}

void P2PSocketsHost::OnChannelClosing() {
  BrowserMessageFilter::OnChannelClosing();

  // Since the IPC channel is gone, close pending connections.
  for (IDMap<P2PSocketHost, IDMapOwnPointer>::iterator i(&sockets_);
       !i.IsAtEnd(); i.Advance()) {
    sockets_.Remove(i.GetCurrentKey());
  }
}

void P2PSocketsHost::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

bool P2PSocketsHost::OnMessageReceived(const IPC::Message& message,
                                       bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(P2PSocketsHost, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(P2PHostMsg_CreateSocket, OnCreateSocket)
    IPC_MESSAGE_HANDLER(P2PHostMsg_Send, OnSend)
    IPC_MESSAGE_HANDLER(P2PHostMsg_DestroySocket, OnDestroySocket)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

void P2PSocketsHost::OnCreateSocket(
    const IPC::Message& msg, P2PSocketType type, int socket_id,
    P2PSocketAddress remote_address) {
  if (sockets_.Lookup(socket_id)) {
    LOG(ERROR) << "Received P2PHostMsg_CreateSocket for socket "
        "that already exists.";
    return;
  }

  if (type != P2P_SOCKET_UDP) {
    Send(new P2PMsg_OnError(msg.routing_id(), socket_id));
    return;
  }

  scoped_ptr<P2PSocketHost> socket(
      P2PSocketHost::Create(this, msg.routing_id(), socket_id, type));

  if (!socket.get()) {
    Send(new P2PMsg_OnError(msg.routing_id(), socket_id));
    return;
  }

  if (socket->Init()) {
    sockets_.AddWithID(socket.release(), socket_id);
  }
}

void P2PSocketsHost::OnSend(const IPC::Message& msg, int socket_id,
                            P2PSocketAddress socket_address,
                            const std::vector<char>& data) {
  P2PSocketHost* socket = sockets_.Lookup(socket_id);
  if (!socket) {
    LOG(ERROR) << "Received P2PHostMsg_Send for invalid socket_id.";
    return;
  }
  socket->Send(socket_address, data);
}

void P2PSocketsHost::OnDestroySocket(const IPC::Message& msg, int socket_id) {
  sockets_.Remove(socket_id);
}
