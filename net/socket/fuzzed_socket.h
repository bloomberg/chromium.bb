// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_SOCKET_FUZZED_SOCKET_H
#define NET_SOCKET_FUZZED_SOCKET_H

#include <stdint.h>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_piece.h"
#include "net/base/completion_callback.h"
#include "net/base/net_errors.h"
#include "net/log/net_log.h"
#include "net/socket/stream_socket.h"

namespace net {

class IOBuffer;

// A StreamSocket that uses a single block of data to generate responses for use
// with fuzzers. Writes can succeed synchronously or asynchronously, can write
// some or all of the provided data, and can fail with several different errors.
// Reads can do the same, but the read data is also generated from the initial
// input data. The number of bytes written/read from a single call is currently
// capped at 127 bytes.
//
// Reads and writes are executed independently of one another, so to guarantee
// the fuzzer behaves the same across repeated runs with the same input, the
// reads and writes must be done in a deterministic order and for a
// deterministic number of bytes, every time the fuzzer is run with the same
// data.
class FuzzedSocket : public StreamSocket {
 public:
  // |data| must be of length |data_size| and is used as to determine behavior
  // of the FuzzedSocket. It must remain valid until the FuzzedSocket is
  // destroyed.
  FuzzedSocket(const uint8_t* data,
               size_t data_size,
               const BoundNetLog& bound_net_log);
  ~FuzzedSocket() override;

  // Socket implementation:
  int Read(IOBuffer* buf,
           int buf_len,
           const CompletionCallback& callback) override;
  int Write(IOBuffer* buf,
            int buf_len,
            const CompletionCallback& callback) override;
  int SetReceiveBufferSize(int32_t size) override;
  int SetSendBufferSize(int32_t size) override;

  // StreamSocket implementation:
  int Connect(const CompletionCallback& callback) override;
  void Disconnect() override;
  bool IsConnected() const override;
  bool IsConnectedAndIdle() const override;
  int GetPeerAddress(IPEndPoint* address) const override;
  int GetLocalAddress(IPEndPoint* address) const override;
  const BoundNetLog& NetLog() const override;
  void SetSubresourceSpeculation() override;
  void SetOmniboxSpeculation() override;
  bool WasEverUsed() const override;
  void EnableTCPFastOpenIfSupported() override;
  bool WasNpnNegotiated() const override;
  NextProto GetNegotiatedProtocol() const override;
  bool GetSSLInfo(SSLInfo* ssl_info) override;
  void GetConnectionAttempts(ConnectionAttempts* out) const override;
  void ClearConnectionAttempts() override;
  void AddConnectionAttempts(const ConnectionAttempts& attempts) override;
  int64_t GetTotalReceivedBytes() const override;

 private:
  // Returns a uint8_t removed from the back of |data_|. Bytes read from the
  // socket are taken from the front of the stream, so this will keep read bytes
  // more consistent between test runs. If no data is left, returns 0.
  uint8_t ConsumeUint8FromData();

  // Returns a net::Error that can be returned by a read or a write. Reads and
  // writes return basically the same set of errors, at the TCP socket layer.
  // Which error is determined by a call to ConsumeUint8FromData().
  Error ConsumeReadWriteErrorFromData();

  void OnReadComplete(const CompletionCallback& callback, int result);
  void OnWriteComplete(const CompletionCallback& callback, int result);

  // The unconsumed portion of the input data that |this| was created with.
  base::StringPiece data_;

  bool read_pending_ = false;
  bool write_pending_ = false;

  // This is true when the first callback returning an error is pending in the
  // message queue. If true, the socket acts like it's connected until that task
  // is run (Or Disconnect() is called), and reads / writes will return the same
  // error asynchronously, until it becomes false, at which point they'll return
  // it synchronously.
  bool error_pending_ = false;
  // If this is not OK, all reads/writes will fail with this error.
  int net_error_ = ERR_CONNECTION_CLOSED;

  int64_t total_bytes_read_ = 0;
  int64_t total_bytes_written_ = 0;

  BoundNetLog bound_net_log_;

  base::WeakPtrFactory<FuzzedSocket> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(FuzzedSocket);
};

}  // namespace net

#endif  // NET_SOCKET_FUZZED_SOCKET_H
