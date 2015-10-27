// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_PACKET_WRITER_H_
#define BLIMP_NET_PACKET_WRITER_H_

#include <string>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "blimp/net/blimp_net_export.h"
#include "net/base/completion_callback.h"
#include "net/base/net_errors.h"

namespace net {
class DrainableIOBuffer;
class StreamSocket;
}  // namespace net

namespace blimp {

// Writes opaque length-prefixed packets to a StreamSocket.
// The header segment is 32-bit, encoded in network byte order.
// The body segment length is specified in the header (should be capped at
//     kMaxPacketPayloadSizeBytes).
class BLIMP_NET_EXPORT PacketWriter {
 public:
  // |socket|: The socket to write packets to. The caller must ensure |socket|
  // is valid while the reader is in-use (see ReadPacket below).
  explicit PacketWriter(net::StreamSocket* socket);

  ~PacketWriter();

  // Writes a packet of at least one byte in size to |socket_|.
  //
  // Returns net::OK or an error code if the operation executed successfully.
  // Returns ERR_IO_PENDING if the operation will be executed asynchronously.
  //     |cb| is later invoked with net::OK or an error code.
  int WritePacket(scoped_refptr<net::DrainableIOBuffer> data,
                  const net::CompletionCallback& callback);

 private:
  enum class WriteState {
    IDLE,
    HEADER,
    PAYLOAD,
  };

  friend std::ostream& operator<<(std::ostream& out, const WriteState state);

  // State machine implementation.
  // |result| - the result value of the most recent network operation.
  // See comments for WritePacket() for documentation on return values.
  int DoWriteLoop(int result);

  int DoWriteHeader(int result);

  int DoWritePayload(int result);

  // Callback function to be invoked on asynchronous write completion.
  // Invokes |callback_| on packet write completion or on error.
  void OnWriteComplete(int result);

  WriteState write_state_;

  net::StreamSocket* socket_;

  scoped_refptr<net::DrainableIOBuffer> payload_buffer_;
  scoped_refptr<net::DrainableIOBuffer> header_buffer_;
  net::CompletionCallback callback_;

  base::WeakPtrFactory<PacketWriter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PacketWriter);
};

}  // namespace blimp

#endif  // BLIMP_NET_PACKET_WRITER_H_
