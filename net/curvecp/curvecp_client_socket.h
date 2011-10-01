// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CURVECP_CURVECP_CLIENT_SOCKET_H_
#define NET_CURVECP_CURVECP_CLIENT_SOCKET_H_
#pragma once

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
  virtual int Connect(OldCompletionCallback* callback);
  virtual void Disconnect();
  virtual bool IsConnected() const;
  virtual bool IsConnectedAndIdle() const;
  virtual int GetPeerAddress(AddressList* address) const;
  virtual int GetLocalAddress(IPEndPoint* address) const;
  virtual const BoundNetLog& NetLog() const;
  virtual void SetSubresourceSpeculation();
  virtual void SetOmniboxSpeculation();
  virtual bool WasEverUsed() const;
  virtual bool UsingTCPFastOpen() const;
  virtual int64 NumBytesRead() const;
  virtual base::TimeDelta GetConnectTimeMicros() const;

  // Socket methods:
  virtual int Read(IOBuffer* buf, int buf_len, OldCompletionCallback* callback);
  virtual int Write(IOBuffer* buf, int buf_len, OldCompletionCallback* callback);
  virtual bool SetReceiveBufferSize(int32 size);
  virtual bool SetSendBufferSize(int32 size);

 private:
  AddressList addresses_;
  BoundNetLog net_log_;
  Messenger messenger_;
  ClientPacketizer packetizer_;

  DISALLOW_COPY_AND_ASSIGN(CurveCPClientSocket);
};

}  // namespace net

#endif  // NET_CURVECP_CURVECP_CLIENT_SOCKET_H_
