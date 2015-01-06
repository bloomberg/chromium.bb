// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/devtools_bridge/socket_tunnel_connection.h"

#include <stdlib.h>

#include "net/base/io_buffer.h"
#include "net/base/net_errors.h"
#include "net/socket/stream_socket.h"

namespace devtools_bridge {

SocketTunnelConnection::SocketTunnelConnection(int index) : index_(index) {
}

SocketTunnelConnection::~SocketTunnelConnection() {
}

void SocketTunnelConnection::Write(scoped_refptr<net::IOBufferWithSize> chunk) {
  // TODO(serya): While it is unlikely (socket normally much faster than
  // data channel) we should disconnect if too much data buffered.
  buffer_.push_back(chunk);
  if (buffer_.size() == 1) {
    current_ = new net::DrainableIOBuffer(chunk.get(), chunk->size());
    WriteCurrent();
  }
}

void SocketTunnelConnection::BuildControlPacket(char* buffer,
                                                int op_code) {
  static_assert(kControlPacketSizeBytes == 3,
                "kControlPacketSizeBytes should equal 3");
  buffer[0] = kControlConnectionId;
  buffer[1] = op_code;
  buffer[2] = index_ + kMinConnectionId;
}

void SocketTunnelConnection::WriteCurrent() {
  while (true) {
    while(current_->BytesRemaining() > 0) {
      int result = socket()->Write(current_.get(), current_->BytesRemaining(),
          base::Bind(&SocketTunnelConnection::OnWriteComplete,
                     base::Unretained(this)));
      if (result > 0)
        current_->DidConsume(result);
    }
    current_ = NULL;

    buffer_.pop_front();
    if (buffer_.empty())
      return;  // Stop writing.

    net::IOBufferWithSize* chunk = buffer_.front().get();
    current_ = new net::DrainableIOBuffer(chunk, chunk->size());
  }
}

void SocketTunnelConnection::OnWriteComplete(int result) {
  if (result > 0) {
    current_->DidConsume(result);
    WriteCurrent();
  }
}

void SocketTunnelConnection::ReadNextChunk() {
  if (!read_buffer_.get()) {
    read_buffer_ = new net::GrowableIOBuffer();
    read_buffer_->SetCapacity(kMaxPacketSizeBytes);
  }
  // Header of the data packet.
  *read_buffer_->StartOfBuffer() = index_ + kMinConnectionId;
  read_buffer_->set_offset(1);

  int result = socket()->Read(
      read_buffer_.get(),
      read_buffer_->RemainingCapacity(),
      base::Bind(&SocketTunnelConnection::OnReadComplete,
          base::Unretained(this)));
  if (result == net::ERR_IO_PENDING)
    return;
  else
    OnReadComplete(result);
}

void SocketTunnelConnection::OnReadComplete(int result) {
  if (result > 0) {
    OnDataPacketRead(read_buffer_->StartOfBuffer(),
                     read_buffer_->offset() + result);
  } else {
    OnReadError(result);
  }
}

}  // namespace devtools_bridge
