// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/p2p/socket_dispatcher_host.h"

#include "content/browser/renderer_host/p2p/socket_host.h"
#include "content/common/p2p_messages.h"

namespace {

// This function returns address of the first IPv4 enabled network
// interface it finds. This address is used for all sockets.
//
// TODO(sergeyu): This approach works only in the simplest case when
// host has only one network connection. Instead of binding all
// connections to this interface we must provide list of interfaces to
// the renderer, and let the PortAllocater in the renderer process
// choose local address.
bool GetLocalAddress(net::IPEndPoint* addr) {
  net::NetworkInterfaceList networks;
  if (!GetNetworkList(&networks))
    return false;

  for (net::NetworkInterfaceList::iterator it = networks.begin();
       it != networks.end(); ++it) {
    if (it->address.size() == net::kIPv4AddressSize) {
      *addr = net::IPEndPoint(it->address, 0);
      return true;
    }
  }

  return false;
}

}  // namespace

P2PSocketDispatcherHost::P2PSocketDispatcherHost() {
}

P2PSocketDispatcherHost::~P2PSocketDispatcherHost() {
}

void P2PSocketDispatcherHost::OnChannelClosing() {
  BrowserMessageFilter::OnChannelClosing();

  // Since the IPC channel is gone, close pending connections.
  for (IDMap<P2PSocketHost, IDMapOwnPointer>::iterator i(&sockets_);
       !i.IsAtEnd(); i.Advance()) {
    sockets_.Remove(i.GetCurrentKey());
  }
}

void P2PSocketDispatcherHost::OnDestruct() const {
  BrowserThread::DeleteOnIOThread::Destruct(this);
}

bool P2PSocketDispatcherHost::OnMessageReceived(const IPC::Message& message,
                                       bool* message_was_ok) {
  bool handled = true;
  IPC_BEGIN_MESSAGE_MAP_EX(P2PSocketDispatcherHost, message, *message_was_ok)
    IPC_MESSAGE_HANDLER(P2PHostMsg_CreateSocket, OnCreateSocket)
    IPC_MESSAGE_HANDLER(P2PHostMsg_Send, OnSend)
    IPC_MESSAGE_HANDLER(P2PHostMsg_DestroySocket, OnDestroySocket)
    IPC_MESSAGE_UNHANDLED(handled = false)
  IPC_END_MESSAGE_MAP_EX()
  return handled;
}

void P2PSocketDispatcherHost::OnCreateSocket(
    const IPC::Message& msg, P2PSocketType type, int socket_id,
    const net::IPEndPoint& remote_address) {
  BrowserThread::PostTask(
      BrowserThread::FILE, FROM_HERE, NewRunnableMethod(
          this, &P2PSocketDispatcherHost::GetLocalAddressAndCreateSocket,
          msg.routing_id(), type, socket_id, remote_address));
}

void P2PSocketDispatcherHost::GetLocalAddressAndCreateSocket(
    int routing_id, P2PSocketType type, int socket_id,
    const net::IPEndPoint& remote_address) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  net::IPEndPoint local_address;
  if (!GetLocalAddress(&local_address)) {
    LOG(ERROR) << "Failed to get local network address.";
    BrowserThread::PostTask(
        BrowserThread::IO, FROM_HERE,
        NewRunnableMethod(this, &P2PSocketDispatcherHost::Send,
                          new P2PMsg_OnError(routing_id, socket_id)));
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::IO, FROM_HERE,
      NewRunnableMethod(this, &P2PSocketDispatcherHost::FinishCreateSocket,
                        routing_id, local_address, type, socket_id,
                        remote_address));
}

void P2PSocketDispatcherHost::FinishCreateSocket(
    int routing_id, const net::IPEndPoint& local_address, P2PSocketType type,
    int socket_id, const net::IPEndPoint& remote_address) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::IO));

  if (sockets_.Lookup(socket_id)) {
    LOG(ERROR) << "Received P2PHostMsg_CreateSocket for socket "
        "that already exists.";
    return;
  }

  if (type != P2P_SOCKET_UDP) {
    Send(new P2PMsg_OnError(routing_id, socket_id));
    return;
  }

  scoped_ptr<P2PSocketHost> socket(
      P2PSocketHost::Create(this, routing_id, socket_id, type));

  if (!socket.get()) {
    Send(new P2PMsg_OnError(routing_id, socket_id));
    return;
  }

  if (socket->Init(local_address)) {
    sockets_.AddWithID(socket.release(), socket_id);
  }
}

void P2PSocketDispatcherHost::OnSend(const IPC::Message& msg, int socket_id,
                                     const net::IPEndPoint& socket_address,
                                     const std::vector<char>& data) {
  P2PSocketHost* socket = sockets_.Lookup(socket_id);
  if (!socket) {
    LOG(ERROR) << "Received P2PHostMsg_Send for invalid socket_id.";
    return;
  }
  socket->Send(socket_address, data);
}

void P2PSocketDispatcherHost::OnDestroySocket(const IPC::Message& msg,
                                              int socket_id) {
  sockets_.Remove(socket_id);
}
