// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JINGLE_GLUE_PSEUDOTCP_ADAPTER_H_
#define JINGLE_GLUE_PSEUDOTCP_ADAPTER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "net/base/net_log.h"
#include "net/socket/stream_socket.h"
#include "third_party/webrtc/p2p/base/pseudotcp.h"

namespace jingle_glue {

// PseudoTcpAdapter adapts a connectionless net::Socket to a connection-
// oriented net::StreamSocket using PseudoTcp.  Because net::StreamSockets
// can be deleted during callbacks, while PseudoTcp cannot, the core of the
// PseudoTcpAdapter is reference counted, with a reference held by the
// adapter, and an additional reference held on the stack during callbacks.
class PseudoTcpAdapter : public net::StreamSocket, base::NonThreadSafe {
 public:
  // Creates an adapter for the supplied Socket.  |socket| should already
  // be ready for use, and ownership of it will be assumed by the adapter.
  explicit PseudoTcpAdapter(net::Socket* socket);
  ~PseudoTcpAdapter() override;

  // net::Socket implementation.
  int Read(net::IOBuffer* buffer,
           int buffer_size,
           const net::CompletionCallback& callback) override;
  int Write(net::IOBuffer* buffer,
            int buffer_size,
            const net::CompletionCallback& callback) override;
  int SetReceiveBufferSize(int32 size) override;
  int SetSendBufferSize(int32 size) override;

  // net::StreamSocket implementation.
  int Connect(const net::CompletionCallback& callback) override;
  void Disconnect() override;
  bool IsConnected() const override;
  bool IsConnectedAndIdle() const override;
  int GetPeerAddress(net::IPEndPoint* address) const override;
  int GetLocalAddress(net::IPEndPoint* address) const override;
  const net::BoundNetLog& NetLog() const override;
  void SetSubresourceSpeculation() override;
  void SetOmniboxSpeculation() override;
  bool WasEverUsed() const override;
  bool UsingTCPFastOpen() const override;
  bool WasNpnNegotiated() const override;
  net::NextProto GetNegotiatedProtocol() const override;
  bool GetSSLInfo(net::SSLInfo* ssl_info) override;

  // Set the delay for sending ACK.
  void SetAckDelay(int delay_ms);

  // Set whether Nagle's algorithm is enabled.
  void SetNoDelay(bool no_delay);

  // When write_waits_for_send flag is set to true the Write() method
  // will wait until the data is sent to the remote end before the
  // write completes (it still doesn't wait until the data is received
  // and acknowledged by the remote end). Otherwise write completes
  // after the data has been copied to the send buffer.
  //
  // This flag is useful in cases when the sender needs to get
  // feedback from the connection when it is congested. E.g. remoting
  // host uses this feature to adjust screen capturing rate according
  // to the available bandwidth. In the same time it may negatively
  // impact performance in some cases. E.g. when the sender writes one
  // byte at a time then each byte will always be sent in a separate
  // packet.
  //
  // TODO(sergeyu): Remove this flag once remoting has a better
  // flow-control solution.
  void SetWriteWaitsForSend(bool write_waits_for_send);

 private:
  class Core;

  scoped_refptr<Core> core_;

  net::BoundNetLog net_log_;

  DISALLOW_COPY_AND_ASSIGN(PseudoTcpAdapter);
};

}  // namespace jingle_glue

#endif  // JINGLE_GLUE_PSEUDOTCP_ADAPTER_H_
