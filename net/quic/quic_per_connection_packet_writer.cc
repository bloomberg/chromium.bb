// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_per_connection_packet_writer.h"

#include "net/quic/quic_server_packet_writer.h"

namespace net {

QuicPerConnectionPacketWriter::QuicPerConnectionPacketWriter(
    QuicServerPacketWriter* shared_writer,
    QuicConnection* connection)
    : shared_writer_(shared_writer),
      connection_(connection),
      weak_factory_(this){
}

QuicPerConnectionPacketWriter::~QuicPerConnectionPacketWriter() {
}

QuicPacketWriter* QuicPerConnectionPacketWriter::shared_writer() const {
  return shared_writer_;
}

WriteResult QuicPerConnectionPacketWriter::WritePacket(
    const char* buffer,
    size_t buf_len,
    const IPAddressNumber& self_address,
    const IPEndPoint& peer_address) {
  return shared_writer_->WritePacketWithCallback(
      buffer,
      buf_len,
      self_address,
      peer_address,
      base::Bind(&QuicPerConnectionPacketWriter::OnWriteComplete,
                 weak_factory_.GetWeakPtr()));
}

bool QuicPerConnectionPacketWriter::IsWriteBlockedDataBuffered() const {
  return shared_writer_->IsWriteBlockedDataBuffered();
}

bool QuicPerConnectionPacketWriter::IsWriteBlocked() const {
  return shared_writer_->IsWriteBlocked();
}

void QuicPerConnectionPacketWriter::SetWritable() {
  shared_writer_->SetWritable();
}

void QuicPerConnectionPacketWriter::OnWriteComplete(WriteResult result) {
  if (result.status == WRITE_STATUS_ERROR) {
    connection_->OnWriteError(result.error_code);
  }
}

}  // namespace net
