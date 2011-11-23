// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CURVECP_CURVECP_CLIENT_SOCKET_H_
#define NET_CURVECP_CURVECP_CLIENT_SOCKET_H_
#pragma once

#include "base/compiler_specific.h"
#include "net/base/completion_callback.h"
#include "net/curvecp/client_packetizer.h"
#include "net/curvecp/messenger.h"
#include "net/socket/stream_socket.h"

namespace net {

// A client socket that uses CurveCP as the transport layer.
class CurveCPClientSocket : public StreamSocket {
 public:
  // The IP address(es) and port number to connect to.  The CurveCP socket will
  // try each IP address in the list until it succeeds in establishing a
  // connection.
  CurveCPClientSocket(const AddressList& addresses,
                      net::NetLog* net_log,
                      const net::NetLog::Source& source);
  virtual ~CurveCPClientSocket();

  // ClientSocket methods:
  virtual int Connect(OldCompletionCallback* callback) OVERRIDE;
  virtual void Disconnect() OVERRIDE;
  virtual bool IsConnected() const OVERRIDE;
  virtual bool IsConnectedAndIdle() const OVERRIDE;
  virtual int GetPeerAddress(AddressList* address) const OVERRIDE;
  virtual int GetLocalAddress(IPEndPoint* address) const OVERRIDE;
  virtual const BoundNetLog& NetLog() const OVERRIDE;
  virtual void SetSubresourceSpeculation() OVERRIDE;
  virtual void SetOmniboxSpeculation() OVERRIDE;
  virtual bool WasEverUsed() const OVERRIDE;
  virtual bool UsingTCPFastOpen() const OVERRIDE;
  virtual int64 NumBytesRead() const OVERRIDE;
  virtual base::TimeDelta GetConnectTimeMicros() const OVERRIDE;

  // Socket methods:
  virtual int Read(IOBuffer* buf,
                   int buf_len,
                   OldCompletionCallback* callback) OVERRIDE;
  virtual int Write(IOBuffer* buf,
                    int buf_len,
                    OldCompletionCallback* callback) OVERRIDE;
  virtual bool SetReceiveBufferSize(int32 size) OVERRIDE;
  virtual bool SetSendBufferSize(int32 size) OVERRIDE;

 private:
  AddressList addresses_;
  BoundNetLog net_log_;
  Messenger messenger_;
  ClientPacketizer packetizer_;

  DISALLOW_COPY_AND_ASSIGN(CurveCPClientSocket);
};

}  // namespace net

#endif  // NET_CURVECP_CURVECP_CLIENT_SOCKET_H_
