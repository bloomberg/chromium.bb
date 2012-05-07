// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/p2p/socket_host_tcp.h"

#include "base/sys_byteorder.h"
#include "content/common/p2p_messages.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/socket/tcp_client_socket.h"

namespace {
const int kReadBufferSize = 4096;
const int kPacketHeaderSize = sizeof(uint16);
}  // namespace

namespace content {

P2PSocketHostTcp::P2PSocketHostTcp(IPC::Message::Sender* message_sender,
                                   int routing_id, int id)
    : P2PSocketHost(message_sender, routing_id, id),
      connected_(false) {
}

P2PSocketHostTcp::~P2PSocketHostTcp() {
  if (state_ == STATE_OPEN) {
    DCHECK(socket_.get());
    socket_.reset();
  }
}

bool P2PSocketHostTcp::InitAccepted(const net::IPEndPoint& remote_address,
                                    net::StreamSocket* socket) {
  DCHECK(socket);
  DCHECK_EQ(state_, STATE_UNINITIALIZED);

  remote_address_ = remote_address;
  socket_.reset(socket);
  state_ = STATE_OPEN;
  DoRead();
  return state_ != STATE_ERROR;
}

bool P2PSocketHostTcp::Init(const net::IPEndPoint& local_address,
                            const net::IPEndPoint& remote_address) {
  DCHECK_EQ(state_, STATE_UNINITIALIZED);

  remote_address_ = remote_address;
  state_ = STATE_CONNECTING;
  scoped_ptr<net::TCPClientSocket> tcp_socket(new net::TCPClientSocket(
      net::AddressList(remote_address),
      NULL, net::NetLog::Source()));
  if (tcp_socket->Bind(local_address) != net::OK) {
    OnError();
    return false;
  }
  socket_.reset(tcp_socket.release());

  int result = socket_->Connect(
      base::Bind(&P2PSocketHostTcp::OnConnected, base::Unretained(this)));
  if (result != net::ERR_IO_PENDING) {
    OnConnected(result);
  }

  return state_ != STATE_ERROR;
}

void P2PSocketHostTcp::OnError() {
  socket_.reset();

  if (state_ == STATE_UNINITIALIZED || state_ == STATE_CONNECTING ||
      state_ == STATE_OPEN) {
    message_sender_->Send(new P2PMsg_OnError(routing_id_, id_));
  }

  state_ = STATE_ERROR;
}

void P2PSocketHostTcp::OnConnected(int result) {
  DCHECK_EQ(state_, STATE_CONNECTING);
  DCHECK_NE(result, net::ERR_IO_PENDING);

  if (result != net::OK) {
    OnError();
    return;
  }

  if (!socket_->SetSendBufferSize(kMaxSendBufferSize)) {
    LOG(WARNING) << "Failed to set send buffer size for TCP socket.";
  }

  net::IPEndPoint address;
  result = socket_->GetLocalAddress(&address);
  if (result < 0) {
    LOG(ERROR) << "P2PSocket::Init(): unable to get local address: "
               << result;
    OnError();
    return;
  }

  VLOG(1) << "Local address: " << address.ToString();
  state_ = STATE_OPEN;
  message_sender_->Send(new P2PMsg_OnSocketCreated(routing_id_, id_, address));
  DoRead();
}

void P2PSocketHostTcp::DoRead() {
  int result;
  do {
    if (!read_buffer_) {
      read_buffer_ = new net::GrowableIOBuffer();
      read_buffer_->SetCapacity(kReadBufferSize);
    } else if (read_buffer_->RemainingCapacity() < kReadBufferSize) {
      // Make sure that we always have at least kReadBufferSize of
      // remaining capacity in the read buffer. Normally all packets
      // are smaller than kReadBufferSize, so this is not really
      // required.
      read_buffer_->SetCapacity(read_buffer_->capacity() + kReadBufferSize -
                                read_buffer_->RemainingCapacity());
    }
    result = socket_->Read(read_buffer_, read_buffer_->RemainingCapacity(),
                           base::Bind(&P2PSocketHostTcp::OnRead,
                                      base::Unretained(this)));
    DidCompleteRead(result);
  } while (result > 0);
}

void P2PSocketHostTcp::OnRead(int result) {
  DidCompleteRead(result);
  if (state_ == STATE_OPEN) {
    DoRead();
  }
}

void P2PSocketHostTcp::OnPacket(std::vector<char>& data) {
  if (!connected_) {
    P2PSocketHost::StunMessageType type;
    bool stun = GetStunPacketType(&*data.begin(), data.size(), &type);
    if (stun && IsRequestOrResponse(type)) {
      connected_ = true;
    } else if (!stun || type == STUN_DATA_INDICATION) {
      LOG(ERROR) << "Received unexpected data packet from "
                 << remote_address_.ToString()
                 << " before STUN binding is finished. "
                 << "Terminating connection.";
      OnError();
      return;
    }
  }

  message_sender_->Send(new P2PMsg_OnDataReceived(routing_id_, id_,
                                                  remote_address_, data));
}

void P2PSocketHostTcp::DidCompleteRead(int result) {
  DCHECK_EQ(state_, STATE_OPEN);

  if (result == net::ERR_IO_PENDING) {
    return;
  } else if (result < 0){
    LOG(ERROR) << "Error when reading from TCP socket: " << result;
    OnError();
    return;
  }

  read_buffer_->set_offset(read_buffer_->offset() + result);
  if (read_buffer_->offset() > kPacketHeaderSize) {
    int packet_size = base::NetToHost16(
        *reinterpret_cast<uint16*>(read_buffer_->StartOfBuffer()));
    if (packet_size + kPacketHeaderSize <= read_buffer_->offset()) {
      // We've got a full packet!
      char* start = read_buffer_->StartOfBuffer() + kPacketHeaderSize;
      std::vector<char> data(start, start + packet_size);
      OnPacket(data);

      // Move remaining data to the start of the buffer.
      memmove(read_buffer_->StartOfBuffer(), start + packet_size,
              read_buffer_->offset() - packet_size - kPacketHeaderSize);
      read_buffer_->set_offset(read_buffer_->offset() - packet_size -
                               kPacketHeaderSize);
    }
  }
}

void P2PSocketHostTcp::Send(const net::IPEndPoint& to,
                            const std::vector<char>& data) {
  if (!socket_.get()) {
    // The Send message may be sent after the an OnError message was
    // sent by hasn't been processed the renderer.
    return;
  }

  if (write_buffer_) {
    // Silently drop packet if we haven't finished sending previous
    // packet.
    VLOG(1) << "Dropping TCP packet.";
    return;
  }

  if (!(to == remote_address_)) {
    // Renderer should use this socket only to send data to
    // |remote_address_|.
    NOTREACHED();
    OnError();
    return;
  }

  if (!connected_) {
    P2PSocketHost::StunMessageType type;
    bool stun = GetStunPacketType(&*data.begin(), data.size(), &type);
    if (!stun || type == STUN_DATA_INDICATION) {
      LOG(ERROR) << "Page tried to send a data packet to " << to.ToString()
                 << " before STUN binding is finished.";
      OnError();
      return;
    }
  }

  int size = kPacketHeaderSize + data.size();
  write_buffer_ = new net::DrainableIOBuffer(new net::IOBuffer(size), size);
  *reinterpret_cast<uint16*>(write_buffer_->data()) =
      base::HostToNet16(data.size());
  memcpy(write_buffer_->data() + kPacketHeaderSize, &data[0], data.size());

  DoWrite();
}

void P2PSocketHostTcp::DoWrite() {
  while (true) {
    int result = socket_->Write(write_buffer_, write_buffer_->BytesRemaining(),
                                base::Bind(&P2PSocketHostTcp::OnWritten,
                                           base::Unretained(this)));
    if (result >= 0) {
      write_buffer_->DidConsume(result);
      if (write_buffer_->BytesRemaining() == 0) {
        write_buffer_ = NULL;
        break;
      }
    } else {
      if (result != net::ERR_IO_PENDING) {
        LOG(ERROR) << "Error when sending data in TCP socket: " << result;
        OnError();
      }
      break;
    }
  }
}

void P2PSocketHostTcp::OnWritten(int result) {
  DCHECK(write_buffer_);
  DCHECK_NE(result, net::ERR_IO_PENDING);

  if (result < 0) {
    DCHECK(result != net::ERR_IO_PENDING);
    LOG(ERROR) << "Error when sending data in TCP socket: " << result;
    OnError();
    return;
  }

  write_buffer_->DidConsume(result);
  if (write_buffer_->BytesRemaining() == 0) {
    write_buffer_ = NULL;
  } else {
    DoWrite();
  }
}

P2PSocketHost* P2PSocketHostTcp::AcceptIncomingTcpConnection(
    const net::IPEndPoint& remote_address, int id) {
  NOTREACHED();
  OnError();
  return NULL;
}

} // namespace content
