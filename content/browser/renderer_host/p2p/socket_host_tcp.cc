// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/p2p/socket_host_tcp.h"

#include "base/sys_byteorder.h"
#include "content/common/p2p_messages.h"
#include "ipc/ipc_sender.h"
#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/base/net_util.h"
#include "net/socket/tcp_client_socket.h"

namespace {

typedef uint16 PacketLength;
const int kPacketHeaderSize = sizeof(PacketLength);
const int kReadBufferSize = 4096;
const int kPacketLengthOffset = 2;
const int kTurnChannelDataHeaderSize = 4;

}  // namespace

namespace content {

P2PSocketHostTcpBase::P2PSocketHostTcpBase(IPC::Sender* message_sender,
                                           int id)
    : P2PSocketHost(message_sender, id),
      write_pending_(false),
      connected_(false) {
}

P2PSocketHostTcpBase::~P2PSocketHostTcpBase() {
  if (state_ == STATE_OPEN) {
    DCHECK(socket_.get());
    socket_.reset();
  }
}

bool P2PSocketHostTcpBase::InitAccepted(const net::IPEndPoint& remote_address,
                                        net::StreamSocket* socket) {
  DCHECK(socket);
  DCHECK_EQ(state_, STATE_UNINITIALIZED);

  remote_address_ = remote_address;
  socket_.reset(socket);
  state_ = STATE_OPEN;
  DoRead();
  return state_ != STATE_ERROR;
}

bool P2PSocketHostTcpBase::Init(const net::IPEndPoint& local_address,
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

void P2PSocketHostTcpBase::OnError() {
  socket_.reset();

  if (state_ == STATE_UNINITIALIZED || state_ == STATE_CONNECTING ||
      state_ == STATE_OPEN) {
    message_sender_->Send(new P2PMsg_OnError(id_));
  }

  state_ = STATE_ERROR;
}

void P2PSocketHostTcpBase::OnConnected(int result) {
  DCHECK_EQ(state_, STATE_CONNECTING);
  DCHECK_NE(result, net::ERR_IO_PENDING);

  if (result != net::OK) {
    OnError();
    return;
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
  message_sender_->Send(new P2PMsg_OnSocketCreated(id_, address));
  DoRead();
}

void P2PSocketHostTcpBase::DoRead() {
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

void P2PSocketHostTcpBase::OnRead(int result) {
  DidCompleteRead(result);
  if (state_ == STATE_OPEN) {
    DoRead();
  }
}

void P2PSocketHostTcpBase::OnPacket(const std::vector<char>& data) {
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

  message_sender_->Send(new P2PMsg_OnDataReceived(id_, remote_address_, data));
}

void P2PSocketHostTcpBase::Send(const net::IPEndPoint& to,
                                const std::vector<char>& data) {
  if (!socket_) {
    // The Send message may be sent after the an OnError message was
    // sent by hasn't been processed the renderer.
    return;
  }

  if (!(to == remote_address_)) {
    // Renderer should use this socket only to send data to |remote_address_|.
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

  DoSend(to, data);
}

void P2PSocketHostTcpBase::WriteOrQueue(
    scoped_refptr<net::DrainableIOBuffer>& buffer) {
  if (write_buffer_) {
    write_queue_.push(buffer);
    return;
  }

  write_buffer_ = buffer;
  DoWrite();
}

void P2PSocketHostTcpBase::DoWrite() {
  while (write_buffer_ && state_ == STATE_OPEN && !write_pending_) {
    int result = socket_->Write(write_buffer_, write_buffer_->BytesRemaining(),
                                base::Bind(&P2PSocketHostTcp::OnWritten,
                                           base::Unretained(this)));
    HandleWriteResult(result);
  }
}

void P2PSocketHostTcpBase::OnWritten(int result) {
  DCHECK(write_pending_);
  DCHECK_NE(result, net::ERR_IO_PENDING);

  write_pending_ = false;
  HandleWriteResult(result);
  DoWrite();
}

void P2PSocketHostTcpBase::HandleWriteResult(int result) {
  DCHECK(write_buffer_);
  if (result >= 0) {
    write_buffer_->DidConsume(result);
    if (write_buffer_->BytesRemaining() == 0) {
      message_sender_->Send(new P2PMsg_OnSendComplete(id_));
      if (write_queue_.empty()) {
        write_buffer_ = NULL;
      } else {
        write_buffer_ = write_queue_.front();
        write_queue_.pop();
      }
    }
  } else if (result == net::ERR_IO_PENDING) {
    write_pending_ = true;
  } else {
    LOG(ERROR) << "Error when sending data in TCP socket: " << result;
    OnError();
  }
}

P2PSocketHost* P2PSocketHostTcpBase::AcceptIncomingTcpConnection(
    const net::IPEndPoint& remote_address, int id) {
  NOTREACHED();
  OnError();
  return NULL;
}

void P2PSocketHostTcpBase::DidCompleteRead(int result) {
  DCHECK_EQ(state_, STATE_OPEN);

  if (result == net::ERR_IO_PENDING) {
    return;
  } else if (result < 0) {
    LOG(ERROR) << "Error when reading from TCP socket: " << result;
    OnError();
    return;
  }

  read_buffer_->set_offset(read_buffer_->offset() + result);
  char* head = read_buffer_->StartOfBuffer();  // Purely a convenience.
  int pos = 0;
  while (pos <= read_buffer_->offset() && state_ == STATE_OPEN) {
    int consumed = ProcessInput(head + pos, read_buffer_->offset() - pos);
    if (!consumed)
      break;
    pos += consumed;
  }
  // We've consumed all complete packets from the buffer; now move any remaining
  // bytes to the head of the buffer and set offset to reflect this.
  if (pos && pos <= read_buffer_->offset()) {
    memmove(head, head + pos, read_buffer_->offset() - pos);
    read_buffer_->set_offset(read_buffer_->offset() - pos);
  }
}

P2PSocketHostTcp::P2PSocketHostTcp(IPC::Sender* message_sender, int id)
    : P2PSocketHostTcpBase(message_sender, id) {
}

P2PSocketHostTcp::~P2PSocketHostTcp() {
}

int P2PSocketHostTcp::ProcessInput(char* input, int input_len) {
  if (input_len < kPacketHeaderSize)
    return 0;
  int packet_size = base::NetToHost16(*reinterpret_cast<uint16*>(input));
  if (input_len < packet_size + kPacketHeaderSize)
    return 0;

  int consumed = kPacketHeaderSize;
  char* cur = input + consumed;
  std::vector<char> data(cur, cur + packet_size);
  OnPacket(data);
  consumed += packet_size;
  return consumed;
}

void P2PSocketHostTcp::DoSend(const net::IPEndPoint& to,
                              const std::vector<char>& data) {
  int size = kPacketHeaderSize + data.size();
  scoped_refptr<net::DrainableIOBuffer> buffer =
      new net::DrainableIOBuffer(new net::IOBuffer(size), size);
  *reinterpret_cast<uint16*>(buffer->data()) = base::HostToNet16(data.size());
  memcpy(buffer->data() + kPacketHeaderSize, &data[0], data.size());

  WriteOrQueue(buffer);
}

// P2PSocketHostStunTcp
P2PSocketHostStunTcp::P2PSocketHostStunTcp(IPC::Sender* message_sender,
                                           int id)
    : P2PSocketHostTcpBase(message_sender, id) {
}

P2PSocketHostStunTcp::~P2PSocketHostStunTcp() {
}

int P2PSocketHostStunTcp::ProcessInput(char* input, int input_len) {
  if (input_len < kPacketHeaderSize + kPacketLengthOffset)
    return 0;

  int pad_bytes;
  int packet_size = GetExpectedPacketSize(
      input, input_len, &pad_bytes);

  if (input_len < packet_size + pad_bytes)
    return 0;

  // We have a complete packet. Read through it.
  int consumed = 0;
  char* cur = input;
  std::vector<char> data(cur, cur + packet_size);
  OnPacket(data);
  consumed += packet_size;
  consumed += pad_bytes;
  return consumed;
}

void P2PSocketHostStunTcp::DoSend(const net::IPEndPoint& to,
                                  const std::vector<char>& data) {
  // Each packet is expected to have header (STUN/TURN ChannelData), where
  // header contains message type and and length of message.
  if (data.size() < kPacketHeaderSize + kPacketLengthOffset) {
    NOTREACHED();
    OnError();
    return;
  }

  int pad_bytes;
  size_t expected_len = GetExpectedPacketSize(
      &data[0], data.size(), &pad_bytes);

  // Accepts only complete STUN/TURN packets.
  if (data.size() != expected_len) {
    NOTREACHED();
    OnError();
    return;
  }

  // Add any pad bytes to the total size.
  int size = data.size() + pad_bytes;

  scoped_refptr<net::DrainableIOBuffer> buffer =
      new net::DrainableIOBuffer(new net::IOBuffer(size), size);
  memcpy(buffer->data(), &data[0], data.size());

  if (pad_bytes) {
    char padding[4] = {0};
    DCHECK_LE(pad_bytes, 4);
    memcpy(buffer->data() + data.size(), padding, pad_bytes);
  }
  WriteOrQueue(buffer);
}

int P2PSocketHostStunTcp::GetExpectedPacketSize(
    const char* data, int len, int* pad_bytes) {
  DCHECK_LE(kTurnChannelDataHeaderSize, len);
  // Both stun and turn had length at offset 2.
  int packet_size = base::NetToHost16(*reinterpret_cast<const uint16*>(
      data + kPacketLengthOffset));

  // Get packet type (STUN or TURN).
  uint16 msg_type = base::NetToHost16(*reinterpret_cast<const uint16*>(data));

  *pad_bytes = 0;
  // Add heder length to packet length.
  if ((msg_type & 0xC000) == 0) {
    packet_size += kStunHeaderSize;
  } else {
    packet_size += kTurnChannelDataHeaderSize;
    // Calculate any padding if present.
    if (packet_size % 4)
      *pad_bytes = 4 - packet_size % 4;
  }
  return packet_size;
}

}  // namespace content
