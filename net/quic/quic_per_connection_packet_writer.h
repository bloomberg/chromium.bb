// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_PER_CONNECTION_PACKET_WRITER_H_
#define NET_QUIC_QUIC_PER_CONNECTION_PACKET_WRITER_H_

#include "base/memory/weak_ptr.h"
#include "net/quic/quic_connection.h"
#include "net/quic/quic_packet_writer.h"

namespace net {

class QuicServerPacketWriter;

// A connection-specific packet writer that notifies its connection when its
// writes to the shared QuicServerPacketWriter complete.
// This class is necessary because multiple connections can share the same
// QuicServerPacketWriter, so it has no way to know which connection to notify.
class QuicPerConnectionPacketWriter : public QuicPacketWriter {
 public:
  // Does not take ownership of |shared_writer| or |connection|.
  QuicPerConnectionPacketWriter(QuicServerPacketWriter* shared_writer,
                                QuicConnection* connection);
  virtual ~QuicPerConnectionPacketWriter();

  QuicPacketWriter* shared_writer() const;
  QuicConnection* connection() const { return connection_; }

  // Default implementation of the QuicPacketWriter interface: Passes everything
  // to |shared_writer_|.
  virtual WriteResult WritePacket(const char* buffer,
                                  size_t buf_len,
                                  const IPAddressNumber& self_address,
                                  const IPEndPoint& peer_address) OVERRIDE;
  virtual bool IsWriteBlockedDataBuffered() const OVERRIDE;
  virtual bool IsWriteBlocked() const OVERRIDE;
  virtual void SetWritable() OVERRIDE;

 private:
  void OnWriteComplete(WriteResult result);

  QuicServerPacketWriter* shared_writer_;  // Not owned.
  QuicConnection* connection_;  // Not owned.

  base::WeakPtrFactory<QuicPerConnectionPacketWriter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(QuicPerConnectionPacketWriter);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_PER_CONNECTION_PACKET_WRITER_H_
