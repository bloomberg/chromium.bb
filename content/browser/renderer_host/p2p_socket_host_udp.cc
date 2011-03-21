// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/p2p_socket_host_udp.h"

#include "content/browser/renderer_host/p2p_sockets_host.h"
#include "content/common/p2p_messages.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"

namespace {

const int kReadBufferSize = 65536;

}  // namespace

P2PSocketHostUdp::P2PSocketHostUdp(P2PSocketsHost* host, int routing_id, int id)
    : P2PSocketHost(host, routing_id, id),
      state_(STATE_UNINITIALIZED),
      send_pending_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          recv_callback_(this, &P2PSocketHostUdp::OnRecv)),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          send_callback_(this, &P2PSocketHostUdp::OnSend)) {
}

P2PSocketHostUdp::~P2PSocketHostUdp() {
  if (state_ == STATE_OPEN) {
    DCHECK(socket_.get());
    socket_.reset();
  }
}

bool P2PSocketHostUdp::Init(const net::IPEndPoint& local_address) {
  net::UDPServerSocket* socket = new net::UDPServerSocket(
      NULL, net::NetLog::Source());
  socket_.reset(socket);

  int result = socket_->Listen(local_address);
  if (result < 0) {
    LOG(ERROR) << "bind() failed: " << result;
    OnError();
    return false;
  }

  net::IPEndPoint address;
  result = socket_->GetLocalAddress(&address);
  if (result < 0) {
    LOG(ERROR) << "P2PSocket::Init(): unable to get local address: "
               << result;
    OnError();
    return false;
  }

  VLOG(1) << "Local address: " << address.ToString();

  state_ = STATE_OPEN;

  recv_buffer_ = new net::IOBuffer(kReadBufferSize);
  DoRead();

  host_->Send(new P2PMsg_OnSocketCreated(routing_id_, id_, address));

  return true;
}

void P2PSocketHostUdp::OnError() {
  socket_.reset();

  if (state_ == STATE_UNINITIALIZED || state_ == STATE_OPEN)
    host_->Send(new P2PMsg_OnError(routing_id_, id_));

  state_ = STATE_ERROR;
}

void P2PSocketHostUdp::DoRead() {
  int result;
  do {
    result = socket_->RecvFrom(recv_buffer_, kReadBufferSize, &recv_address_,
                               &recv_callback_);
    DidCompleteRead(result);
  } while (result > 0);
}

void P2PSocketHostUdp::OnRecv(int result) {
  DidCompleteRead(result);
  if (state_ == STATE_OPEN) {
    DoRead();
  }
}

void P2PSocketHostUdp::DidCompleteRead(int result) {
  DCHECK_EQ(state_, STATE_OPEN);

  if (result > 0) {
    std::vector<char> data(recv_buffer_->data(), recv_buffer_->data() + result);
    host_->Send(new P2PMsg_OnDataReceived(routing_id_, id_,
                                          recv_address_, data));
  } else if (result < 0 && result != net::ERR_IO_PENDING) {
    LOG(ERROR) << "Error when reading from UDP socket: " << result;
    OnError();
  }
}

void P2PSocketHostUdp::Send(const net::IPEndPoint& socket_address,
                            const std::vector<char>& data) {
  if (send_pending_) {
    // Silently drop packet if previous send hasn't finished.
    VLOG(1) << "Dropping UDP packet.";
    return;
  }

  scoped_refptr<net::IOBuffer> buffer = new net::IOBuffer(data.size());
  memcpy(buffer->data(), &data.begin()[0], data.size());
  int result = socket_->SendTo(buffer, data.size(), socket_address,
                               &send_callback_);
  if (result == net::ERR_IO_PENDING) {
    send_pending_ = true;
  } else if (result < 0) {
    LOG(ERROR) << "Error when sending data in UDP socket: " << result;
    OnError();
  }
}

void P2PSocketHostUdp::OnSend(int result) {
  DCHECK(send_pending_);
  DCHECK_NE(result, net::ERR_IO_PENDING);

  send_pending_ = false;
  if (result < 0)
    OnError();
}
