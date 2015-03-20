// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_packet_reader.h"

#include "base/metrics/histogram.h"
#include "net/base/net_errors.h"

namespace net {

QuicPacketReader::QuicPacketReader(DatagramClientSocket* socket,
                                   Visitor* visitor,
                                   const BoundNetLog& net_log)
    : socket_(socket),
      visitor_(visitor),
      read_pending_(false),
      num_packets_read_(0),
      read_buffer_(new IOBufferWithSize(kMaxPacketSize)),
      net_log_(net_log),
      weak_factory_(this) {
}

QuicPacketReader::~QuicPacketReader() {
}

void QuicPacketReader::StartReading() {
  if (read_pending_)
    return;

  DCHECK(socket_);
  read_pending_ = true;
  int rv = socket_->Read(read_buffer_.get(), read_buffer_->size(),
                         base::Bind(&QuicPacketReader::OnReadComplete,
                                    weak_factory_.GetWeakPtr()));
  UMA_HISTOGRAM_BOOLEAN("Net.QuicSession.AsyncRead", rv == ERR_IO_PENDING);
  if (rv == ERR_IO_PENDING) {
    num_packets_read_ = 0;
    return;
  }

  if (++num_packets_read_ > 32) {
    num_packets_read_ = 0;
    // Data was read, process it.
    // Schedule the work through the message loop to 1) prevent infinite
    // recursion and 2) avoid blocking the thread for too long.
    base::MessageLoop::current()->PostTask(
        FROM_HERE, base::Bind(&QuicPacketReader::OnReadComplete,
                              weak_factory_.GetWeakPtr(), rv));
  } else {
    OnReadComplete(rv);
  }
}

void QuicPacketReader::OnReadComplete(int result) {
  read_pending_ = false;
  if (result == 0)
    result = ERR_CONNECTION_CLOSED;

  if (result < 0) {
    visitor_->OnReadError(result);
    return;
  }

  QuicEncryptedPacket packet(read_buffer_->data(), result);
  IPEndPoint local_address;
  IPEndPoint peer_address;
  socket_->GetLocalAddress(&local_address);
  socket_->GetPeerAddress(&peer_address);
  if (!visitor_->OnPacket(packet, local_address, peer_address))
    return;

  StartReading();
}

}  // namespace net
