// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef JINGLE_GLUE_PSEUDOTCP_ADAPTER_H_
#define JINGLE_GLUE_PSEUDOTCP_ADAPTER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer.h"
#include "base/threading/non_thread_safe.h"
#include "net/base/net_log.h"
#include "net/socket/stream_socket.h"
#include "third_party/libjingle/source/talk/p2p/base/pseudotcp.h"

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
  PseudoTcpAdapter(net::Socket* socket);
  virtual ~PseudoTcpAdapter();

  // net::Socket implementation.
  virtual int Read(net::IOBuffer* buffer, int buffer_size,
                   net::OldCompletionCallback* callback) OVERRIDE;
  virtual int Write(net::IOBuffer* buffer, int buffer_size,
                    net::OldCompletionCallback* callback) OVERRIDE;
  virtual bool SetReceiveBufferSize(int32 size) OVERRIDE;
  virtual bool SetSendBufferSize(int32 size) OVERRIDE;

  // net::StreamSocket implementation.
  virtual int Connect(net::OldCompletionCallback* callback) OVERRIDE;
  virtual void Disconnect() OVERRIDE;
  virtual bool IsConnected() const OVERRIDE;
  virtual bool IsConnectedAndIdle() const OVERRIDE;
  virtual int GetPeerAddress(net::AddressList* address) const OVERRIDE;
  virtual int GetLocalAddress(net::IPEndPoint* address) const OVERRIDE;
  virtual const net::BoundNetLog& NetLog() const OVERRIDE;
  virtual void SetSubresourceSpeculation() OVERRIDE;
  virtual void SetOmniboxSpeculation() OVERRIDE;
  virtual bool WasEverUsed() const OVERRIDE;
  virtual bool UsingTCPFastOpen() const OVERRIDE;
  virtual int64 NumBytesRead() const OVERRIDE;
  virtual base::TimeDelta GetConnectTimeMicros() const OVERRIDE;

  // Set the delay for sending ACK.
  void SetAckDelay(int delay_ms);

  // Set whether Nagle's algorithm is enabled.
  void SetNoDelay(bool no_delay);

 private:
  class Core;

  scoped_refptr<Core> core_;

  net::BoundNetLog net_log_;

  DISALLOW_COPY_AND_ASSIGN(PseudoTcpAdapter);
};

}  // namespace jingle_glue

#endif  // JINGLE_GLUE_STREAM_SOCKET_ADAPTER_H_
