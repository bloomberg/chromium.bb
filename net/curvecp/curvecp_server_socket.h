// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_CURVECP_CURVECP_SERVER_SOCKET_H_
#define NET_CURVECP_CURVECP_SERVER_SOCKET_H_
#pragma once

#include "base/compiler_specific.h"
#include "net/base/completion_callback.h"
#include "net/curvecp/connection_key.h"
#include "net/curvecp/protocol.h"
#include "net/curvecp/server_messenger.h"
#include "net/curvecp/server_packetizer.h"
#include "net/socket/stream_socket.h"

namespace net {

// A server socket that uses CurveCP as the transport layer.
class CurveCPServerSocket : public Socket,
                            public ServerMessenger::Acceptor {
 public:
  class Acceptor {
   public:
    virtual ~Acceptor() {}
    virtual void OnAccept(CurveCPServerSocket* new_socket) = 0;
  };

  CurveCPServerSocket(net::NetLog* net_log,
                      const net::NetLog::Source& source);
  virtual ~CurveCPServerSocket();

  int Listen(const IPEndPoint& endpoint, Acceptor* acceptor);
  void Close();

  // Socket methods:
  virtual int Read(IOBuffer* buf,
                   int buf_len,
                   OldCompletionCallback* callback) OVERRIDE;
  virtual int Write(IOBuffer* buf,
                    int buf_len,
                    OldCompletionCallback* callback) OVERRIDE;
  virtual bool SetReceiveBufferSize(int32 size) OVERRIDE;
  virtual bool SetSendBufferSize(int32 size) OVERRIDE;

  // ServerMessenger::Acceptor methods:
  virtual void OnAccept(ConnectionKey key) OVERRIDE;

 private:
  CurveCPServerSocket(const ConnectionKey& key,
                      ServerPacketizer* packetizer_,
                      net::NetLog* net_log,
                      const net::NetLog::Source& source);
  ServerMessenger* messenger() { return &messenger_; }

  BoundNetLog net_log_;
  scoped_refptr<ServerPacketizer> packetizer_;
  ServerMessenger messenger_;
  Acceptor* acceptor_;
  bool is_child_socket_;  // Was this socket accepted from another.
  ConnectionKey key_;

  DISALLOW_COPY_AND_ASSIGN(CurveCPServerSocket);
};

}  // namespace net

#endif  // NET_CURVECP_CURVECP_SERVER_SOCKET_H_
