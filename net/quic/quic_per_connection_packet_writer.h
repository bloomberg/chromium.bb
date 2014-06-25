// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_PER_CONNECTION_PACKET_WRITER_H_
#define NET_QUIC_QUIC_PER_CONNECTION_PACKET_WRITER_H_

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "net/base/ip_endpoint.h"
#include "net/quic/quic_connection.h"
#include "net/quic/quic_packet_writer.h"
#include "net/quic/quic_protocol.h"
#include "net/quic/quic_types.h"

namespace net {

class QuicServerPacketWriter;

// A connection-specific packet writer that notifies its connection when its
// writes to the shared QuicServerPacketWriter complete.
// This class is necessary because multiple connections can share the same
// QuicServerPacketWriter, so it has no way to know which connection to notify.
// TODO(dmz) Try to merge with Chrome's default packet writer
class QuicPerConnectionPacketWriter : public QuicPacketWriter {
 public:
  QuicPerConnectionPacketWriter(QuicServerPacketWriter* writer);
  virtual ~QuicPerConnectionPacketWriter();

  // Set the connection to notify after writes complete.
  void set_connection(QuicConnection* connection) { connection_ = connection; }

  // QuicPacketWriter
  virtual WriteResult WritePacket(const char* buffer,
                                  size_t buf_len,
                                  const IPAddressNumber& self_address,
                                  const IPEndPoint& peer_address) OVERRIDE;
  virtual bool IsWriteBlockedDataBuffered() const OVERRIDE;
  virtual bool IsWriteBlocked() const OVERRIDE;
  virtual void SetWritable() OVERRIDE;

 private:
  void OnWriteComplete(WriteResult result);

  base::WeakPtrFactory<QuicPerConnectionPacketWriter> weak_factory_;
  QuicServerPacketWriter* writer_;  // Not owned.
  QuicConnection* connection_;

  DISALLOW_COPY_AND_ASSIGN(QuicPerConnectionPacketWriter);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_PER_CONNECTION_PACKET_WRITER_H_
