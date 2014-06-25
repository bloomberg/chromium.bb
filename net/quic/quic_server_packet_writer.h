// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_QUIC_SERVER_PACKET_WRITER_H_
#define NET_QUIC_QUIC_SERVER_PACKET_WRITER_H_

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "net/base/ip_endpoint.h"
#include "net/quic/quic_connection.h"
#include "net/quic/quic_packet_writer.h"
#include "net/quic/quic_protocol.h"

namespace net {

class QuicBlockedWriterInterface;
class UDPServerSocket;
struct WriteResult;

// Chrome specific packet writer which uses a UDPServerSocket for writing
// data.
class QuicServerPacketWriter : public QuicPacketWriter {
 public:
  typedef base::Callback<void(WriteResult)> WriteCallback;

  QuicServerPacketWriter(UDPServerSocket* socket,
                         QuicBlockedWriterInterface* blocked_writer);
  virtual ~QuicServerPacketWriter();

  // Use this method to write packets rather than WritePacket:
  // QuicServerPacketWriter requires a callback to exist for every write, which
  // will be called once the write completes.
  virtual WriteResult WritePacketWithCallback(
      const char* buffer,
      size_t buf_len,
      const IPAddressNumber& self_address,
      const IPEndPoint& peer_address,
      WriteCallback callback);

  void OnWriteComplete(int rv);

  // QuicPacketWriter implementation:
  virtual bool IsWriteBlockedDataBuffered() const OVERRIDE;
  virtual bool IsWriteBlocked() const OVERRIDE;
  virtual void SetWritable() OVERRIDE;

 protected:
  // Do not call WritePacket on its own -- use WritePacketWithCallback
  virtual WriteResult WritePacket(const char* buffer,
                                  size_t buf_len,
                                  const IPAddressNumber& self_address,
                                  const IPEndPoint& peer_address) OVERRIDE;
 private:
  base::WeakPtrFactory<QuicServerPacketWriter> weak_factory_;
  UDPServerSocket* socket_;

  // To be notified after every successful asynchronous write.
  QuicBlockedWriterInterface* blocked_writer_;

  // To call once the write completes.
  WriteCallback callback_;

  // Whether a write is currently in flight.
  bool write_blocked_;

  DISALLOW_COPY_AND_ASSIGN(QuicServerPacketWriter);
};

}  // namespace net

#endif  // NET_QUIC_QUIC_SERVER_PACKET_WRITER_H_
