// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_QUIC_CHROMIUM_QUIC_CHROMIUM_PACKET_WRITER_H_
#define NET_QUIC_CHROMIUM_QUIC_CHROMIUM_PACKET_WRITER_H_

#include <stddef.h>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "net/base/io_buffer.h"
#include "net/base/net_export.h"
#include "net/quic/core/quic_connection.h"
#include "net/quic/core/quic_packet_writer.h"
#include "net/quic/core/quic_packets.h"
#include "net/quic/core/quic_types.h"
#include "net/socket/datagram_client_socket.h"

namespace net {

// Chrome specific packet writer which uses a datagram Socket for writing data.
class NET_EXPORT_PRIVATE QuicChromiumPacketWriter : public QuicPacketWriter {
 public:
  // Define a specific IO buffer that can be allocated once, but be
  // assigned new contents and reused, avoiding the alternative of
  // repeated memory allocations.  This packet writer only ever has a
  // single write in flight, a constraint inherited from the interface
  // of the underlying datagram Socket.
  class NET_EXPORT_PRIVATE ReusableIOBuffer : public IOBuffer {
   public:
    explicit ReusableIOBuffer(size_t capacity);

    size_t capacity() const { return capacity_; }
    size_t size() const { return size_; }

    // Does memcpy from |buffer| into this->data(). |buf_len <=
    // capacity()| must be true, |HasOneRef()| must be true.
    void Set(const char* buffer, size_t buf_len);

   private:
    ~ReusableIOBuffer() override;
    size_t capacity_;
    size_t size_;
  };
  // Delegate interface which receives notifications on socket write events.
  class NET_EXPORT_PRIVATE Delegate {
   public:
    // Called when a socket write attempt results in a failure, so
    // that the delegate may recover from it by perhaps rewriting the
    // packet to a different socket. An implementation must return the
    // return value from the rewrite attempt if there is one, and
    // |error_code| otherwise.
    virtual int HandleWriteError(
        int error_code,
        scoped_refptr<ReusableIOBuffer> last_packet) = 0;

    // Called to propagate the final write error to the delegate.
    virtual void OnWriteError(int error_code) = 0;

    // Called when the writer is unblocked due to a write completion.
    virtual void OnWriteUnblocked() = 0;
  };

  QuicChromiumPacketWriter();
  // |socket| must outlive writer.
  explicit QuicChromiumPacketWriter(DatagramClientSocket* socket);
  ~QuicChromiumPacketWriter() override;

  // |delegate| must outlive writer.
  void set_delegate(Delegate* delegate) { delegate_ = delegate; }

  void set_write_blocked(bool write_blocked) { write_blocked_ = write_blocked; }

  // Writes |packet| to the socket and returns the error code from the write.
  WriteResult WritePacketToSocket(scoped_refptr<ReusableIOBuffer> packet);

  // QuicPacketWriter
  WriteResult WritePacket(const char* buffer,
                          size_t buf_len,
                          const QuicIpAddress& self_address,
                          const QuicSocketAddress& peer_address,
                          PerPacketOptions* options) override;
  bool IsWriteBlockedDataBuffered() const override;
  bool IsWriteBlocked() const override;
  void SetWritable() override;
  QuicByteCount GetMaxPacketSize(
      const QuicSocketAddress& peer_address) const override;

  void OnWriteComplete(int rv);

 private:
  void SetPacket(const char* buffer, size_t buf_len);
  WriteResult WritePacketToSocketImpl();
  DatagramClientSocket* socket_;  // Unowned.
  Delegate* delegate_;  // Unowned.
  // Reused for every packet write for the lifetime of the writer.  Is
  // moved to the delegate in the case of a write error.
  scoped_refptr<ReusableIOBuffer> packet_;

  // Whether a write is currently in flight.
  bool write_blocked_;

  CompletionCallback write_callback_;
  base::WeakPtrFactory<QuicChromiumPacketWriter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(QuicChromiumPacketWriter);
};

}  // namespace net

#endif  // NET_QUIC_CHROMIUM_QUIC_CHROMIUM_PACKET_WRITER_H_
