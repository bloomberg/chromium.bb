// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/p2p/socket_host_udp.h"

#include "base/bind.h"
#include "content/common/p2p_messages.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"

namespace {

// UDP packets cannot be bigger than 64k.
const int kReadBufferSize = 65536;

}  // namespace

namespace content {

P2PSocketHostUdp::PendingPacket::PendingPacket(
    const net::IPEndPoint& to, const std::vector<char>& content)
    : to(to),
      data(new net::IOBuffer(content.size())),
      size(content.size()) {
  memcpy(data->data(), &content[0], size);
}

P2PSocketHostUdp::PendingPacket::~PendingPacket() {
}

P2PSocketHostUdp::P2PSocketHostUdp(IPC::Sender* message_sender, int id)
    : P2PSocketHost(message_sender, id),
      socket_(new net::UDPServerSocket(NULL, net::NetLog::Source())),
      send_queue_bytes_(0),
      send_pending_(false) {
}

P2PSocketHostUdp::~P2PSocketHostUdp() {
  if (state_ == STATE_OPEN) {
    DCHECK(socket_.get());
    socket_.reset();
  }
}

bool P2PSocketHostUdp::Init(const net::IPEndPoint& local_address,
                            const net::IPEndPoint& remote_address) {
  DCHECK_EQ(state_, STATE_UNINITIALIZED);

  int result = socket_->Listen(local_address);
  if (result < 0) {
    LOG(ERROR) << "bind() failed: " << result;
    OnError();
    return false;
  }

  net::IPEndPoint address;
  result = socket_->GetLocalAddress(&address);
  if (result < 0) {
    LOG(ERROR) << "P2PSocketHostUdp::Init(): unable to get local address: "
               << result;
    OnError();
    return false;
  }
  VLOG(1) << "Local address: " << address.ToString();

  state_ = STATE_OPEN;

  message_sender_->Send(new P2PMsg_OnSocketCreated(id_, address));

  recv_buffer_ = new net::IOBuffer(kReadBufferSize);
  DoRead();

  return true;
}

void P2PSocketHostUdp::OnError() {
  socket_.reset();
  send_queue_.clear();

  if (state_ == STATE_UNINITIALIZED || state_ == STATE_OPEN)
    message_sender_->Send(new P2PMsg_OnError(id_));

  state_ = STATE_ERROR;
}

void P2PSocketHostUdp::DoRead() {
  int result;
  do {
    result = socket_->RecvFrom(recv_buffer_, kReadBufferSize, &recv_address_,
                               base::Bind(&P2PSocketHostUdp::OnRecv,
                                          base::Unretained(this)));
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

    if (connected_peers_.find(recv_address_) == connected_peers_.end()) {
      P2PSocketHost::StunMessageType type;
      bool stun = GetStunPacketType(&*data.begin(), data.size(), &type);
      if (stun && IsRequestOrResponse(type)) {
        connected_peers_.insert(recv_address_);
      } else if (!stun || type == STUN_DATA_INDICATION) {
        LOG(ERROR) << "Received unexpected data packet from "
                   << recv_address_.ToString()
                   << " before STUN binding is finished.";
        return;
      }
    }

    message_sender_->Send(new P2PMsg_OnDataReceived(id_, recv_address_, data));
  } else if (result < 0 && result != net::ERR_IO_PENDING) {
    LOG(ERROR) << "Error when reading from UDP socket: " << result;
    OnError();
  }
}

void P2PSocketHostUdp::Send(const net::IPEndPoint& to,
                            const std::vector<char>& data) {
  if (!socket_.get()) {
    // The Send message may be sent after the an OnError message was
    // sent by hasn't been processed the renderer.
    return;
  }

  if (connected_peers_.find(to) == connected_peers_.end()) {
    P2PSocketHost::StunMessageType type;
    bool stun = GetStunPacketType(&*data.begin(), data.size(), &type);
    if (!stun || type == STUN_DATA_INDICATION) {
      LOG(ERROR) << "Page tried to send a data packet to " << to.ToString()
                 << " before STUN binding is finished.";
      OnError();
      return;
    }
  }

  if (send_pending_) {
    if (send_queue_bytes_ + static_cast<int>(data.size()) >
        kMaxSendBufferSize) {
      LOG(WARNING) << "Send buffer is full. Dropping a packet.";
      return;
    }
    send_queue_.push_back(PendingPacket(to, data));
    send_queue_bytes_ += data.size();
  } else {
    PendingPacket packet(to, data);
    DoSend(packet);
  }
}

void P2PSocketHostUdp::DoSend(const PendingPacket& packet) {
  int result = socket_->SendTo(packet.data, packet.size, packet.to,
                               base::Bind(&P2PSocketHostUdp::OnSend,
                                          base::Unretained(this)));
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

  // We may get ERR_ADDRESS_UNREACHABLE here if the peer host has a
  // local IP address with the same subnet address as the local
  // host. This error is ingored so that this socket can still be used
  // to try to connect to different candidates.
  if (result < 0 && result != net::ERR_ADDRESS_UNREACHABLE) {
    OnError();
    return;
  }

  // Send next packets if we have them waiting in the buffer.
  while (!send_queue_.empty() && !send_pending_) {
    DoSend(send_queue_.front());
    send_queue_bytes_ -= send_queue_.front().size;
    send_queue_.pop_front();
  }
}

P2PSocketHost* P2PSocketHostUdp::AcceptIncomingTcpConnection(
    const net::IPEndPoint& remote_address, int id) {
  NOTREACHED();
  OnError();
  return NULL;
}

}  // namespace content
