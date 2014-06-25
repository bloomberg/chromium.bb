// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_per_connection_packet_writer.h"

#include "net/quic/quic_server_packet_writer.h"
#include "net/quic/quic_types.h"

namespace net {

QuicPerConnectionPacketWriter::QuicPerConnectionPacketWriter(
    QuicServerPacketWriter* writer)
    : weak_factory_(this), writer_(writer) {
}

QuicPerConnectionPacketWriter::~QuicPerConnectionPacketWriter() {
}

WriteResult QuicPerConnectionPacketWriter::WritePacket(
    const char* buffer,
    size_t buf_len,
    const IPAddressNumber& self_address,
    const IPEndPoint& peer_address) {
  return writer_->WritePacketWithCallback(
      buffer,
      buf_len,
      self_address,
      peer_address,
      base::Bind(&QuicPerConnectionPacketWriter::OnWriteComplete,
                 weak_factory_.GetWeakPtr()));
}

bool QuicPerConnectionPacketWriter::IsWriteBlockedDataBuffered() const {
  return writer_->IsWriteBlockedDataBuffered();
}

bool QuicPerConnectionPacketWriter::IsWriteBlocked() const {
  return writer_->IsWriteBlocked();
}

void QuicPerConnectionPacketWriter::SetWritable() {
  writer_->SetWritable();
}

void QuicPerConnectionPacketWriter::OnWriteComplete(WriteResult result) {
  connection_->OnPacketSent(result);
}

}  // namespace net
