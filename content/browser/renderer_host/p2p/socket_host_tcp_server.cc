// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/p2p/socket_host_tcp_server.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/stl_util.h"
#include "content/browser/renderer_host/p2p/socket_host_tcp.h"
#include "content/common/p2p_messages.h"
#include "net/base/address_list.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/socket/stream_socket.h"

namespace {
const int kListenBacklog = 5;
}  // namespace

namespace content {

P2PSocketHostTcpServer::P2PSocketHostTcpServer(
    IPC::Sender* message_sender, int id)
    : P2PSocketHost(message_sender, id),
      socket_(new net::TCPServerSocket(NULL, net::NetLog::Source())),
      ALLOW_THIS_IN_INITIALIZER_LIST(accept_callback_(
          base::Bind(&P2PSocketHostTcpServer::OnAccepted,
                     base::Unretained(this)))) {
}

P2PSocketHostTcpServer::~P2PSocketHostTcpServer() {
  STLDeleteContainerPairSecondPointers(accepted_sockets_.begin(),
                                       accepted_sockets_.end());

  if (state_ == STATE_OPEN) {
    DCHECK(socket_.get());
    socket_.reset();
  }
}

bool P2PSocketHostTcpServer::Init(const net::IPEndPoint& local_address,
                                  const net::IPEndPoint& remote_address) {
  DCHECK_EQ(state_, STATE_UNINITIALIZED);

  int result = socket_->Listen(local_address, kListenBacklog);
  if (result < 0) {
    LOG(ERROR) << "Listen() failed: " << result;
    OnError();
    return false;
  }

  result = socket_->GetLocalAddress(&local_address_);
  if (result < 0) {
    LOG(ERROR) << "P2PSocketHostTcpServer::Init(): can't to get local address: "
               << result;
    OnError();
    return false;
  }
  VLOG(1) << "Local address: " << local_address_.ToString();

  state_ = STATE_OPEN;
  message_sender_->Send(new P2PMsg_OnSocketCreated(id_, local_address_));
  DoAccept();
  return true;
}

void P2PSocketHostTcpServer::OnError() {
  socket_.reset();

  if (state_ == STATE_UNINITIALIZED || state_ == STATE_OPEN)
    message_sender_->Send(new P2PMsg_OnError(id_));

  state_ = STATE_ERROR;
}

void P2PSocketHostTcpServer::DoAccept() {
  while (true) {
    int result = socket_->Accept(&accept_socket_, accept_callback_);
    if (result == net::ERR_IO_PENDING) {
      break;
    } else {
      HandleAcceptResult(result);
    }
  }
}

void P2PSocketHostTcpServer::HandleAcceptResult(int result) {
  if (result < 0) {
    if (result != net::ERR_IO_PENDING)
      OnError();
    return;
  }

  net::IPEndPoint address;
  if (accept_socket_->GetPeerAddress(&address) != net::OK) {
    LOG(ERROR) << "Failed to get address of an accepted socket.";
    accept_socket_.reset();
    return;
  }
  AcceptedSocketsMap::iterator it = accepted_sockets_.find(address);
  if (it != accepted_sockets_.end())
    delete it->second;

  accepted_sockets_[address] = accept_socket_.release();
  message_sender_->Send(
      new P2PMsg_OnIncomingTcpConnection(id_, address));
}

void P2PSocketHostTcpServer::OnAccepted(int result) {
  HandleAcceptResult(result);
  if (result == net::OK)
    DoAccept();
}

void P2PSocketHostTcpServer::Send(const net::IPEndPoint& to,
                                  const std::vector<char>& data) {
  NOTREACHED();
  OnError();
}

P2PSocketHost* P2PSocketHostTcpServer::AcceptIncomingTcpConnection(
    const net::IPEndPoint& remote_address, int id) {
  AcceptedSocketsMap::iterator it = accepted_sockets_.find(remote_address);
  if (it == accepted_sockets_.end())
    return NULL;

  net::StreamSocket* socket = it->second;
  accepted_sockets_.erase(it);
  scoped_ptr<P2PSocketHostTcp> result(
      new P2PSocketHostTcp(message_sender_, id));
  if (!result->InitAccepted(remote_address, socket))
    return NULL;

  return result.release();
}

}  // namespace content
