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

class PseudoTcpAdapter : public net::StreamSocket,
                         public cricket::IPseudoTcpNotify,
                         public base::NonThreadSafe {
 public:
  // Creates adapter for the specified |socket|. |socket| is assumed
  // to be already connected. Takes ownership of |socket|.
  PseudoTcpAdapter(net::Socket* socket);
  virtual ~PseudoTcpAdapter();

  // net::Socket implementation.
  virtual int Read(net::IOBuffer* buffer, int buffer_size,
                   net::CompletionCallback* callback) OVERRIDE;
  virtual int Write(net::IOBuffer* buffer, int buffer_size,
                    net::CompletionCallback* callback) OVERRIDE;
  virtual bool SetReceiveBufferSize(int32 size) OVERRIDE;
  virtual bool SetSendBufferSize(int32 size) OVERRIDE;

  // net::StreamSocket implementation.
  virtual int Connect(net::CompletionCallback* callback) OVERRIDE;
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

  // cricket::IPseudoTcpNotify implementation.
  virtual void OnTcpOpen(cricket::PseudoTcp* tcp) OVERRIDE;
  virtual void OnTcpReadable(cricket::PseudoTcp* tcp) OVERRIDE;
  virtual void OnTcpWriteable(cricket::PseudoTcp* tcp) OVERRIDE;
  virtual void OnTcpClosed(cricket::PseudoTcp* tcp, uint32 error) OVERRIDE;
  virtual WriteResult TcpWritePacket(cricket::PseudoTcp* tcp,
                                     const char* buffer, size_t len) OVERRIDE;

 private:
  void DoReadFromSocket();
  void HandleReadResults(int result);

  // Callback functions for Read() and Write() in |socket_|
  void OnRead(int result);
  void OnWritten(int result);

  void AdjustClock();
  void HandleTcpClock();

  scoped_ptr<net::Socket> socket_;
  cricket::PseudoTcp pseudotcp_;

  net::CompletionCallback* connect_callback_;
  scoped_refptr<net::IOBuffer> read_buffer_;
  int read_buffer_size_;
  net::CompletionCallback* read_callback_;
  scoped_refptr<net::IOBuffer> write_buffer_;
  int write_buffer_size_;
  net::CompletionCallback* write_callback_;

  bool socket_write_pending_;
  scoped_refptr<net::IOBuffer> socket_read_buffer_;

  net::CompletionCallbackImpl<PseudoTcpAdapter> socket_read_callback_;
  net::CompletionCallbackImpl<PseudoTcpAdapter> socket_write_callback_;

  base::OneShotTimer<PseudoTcpAdapter> timer_;

  net::BoundNetLog net_log_;

  DISALLOW_COPY_AND_ASSIGN(PseudoTcpAdapter);
};

}  // namespace jingle_glue

#endif  // JINGLE_GLUE_STREAM_SOCKET_ADAPTER_H_
