// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/curvecp/curvecp_server_socket.h"
#include "net/curvecp/messenger.h"

namespace net {

CurveCPServerSocket::CurveCPServerSocket(net::NetLog* net_log,
                                         const net::NetLog::Source& source)
    : net_log_(BoundNetLog::Make(net_log, NetLog::SOURCE_SOCKET)),
      packetizer_(new ServerPacketizer()),
      ALLOW_THIS_IN_INITIALIZER_LIST(messenger_(packetizer_.get(), this)),
      acceptor_(NULL),
      is_child_socket_(false) {
}

// Constructor for an accepted socket.
CurveCPServerSocket::CurveCPServerSocket(const ConnectionKey& key,
                                         ServerPacketizer* packetizer,
                                         net::NetLog* net_log,
                                         const net::NetLog::Source& source)
    : net_log_(BoundNetLog::Make(net_log, NetLog::SOURCE_SOCKET)),
      packetizer_(packetizer),
      ALLOW_THIS_IN_INITIALIZER_LIST(messenger_(key, packetizer_.get(), this)),
      acceptor_(NULL),
      is_child_socket_(true),
      key_(key) {
}

CurveCPServerSocket::~CurveCPServerSocket() {
}

int CurveCPServerSocket::Listen(const IPEndPoint& endpoint,
                                Acceptor* acceptor) {
  acceptor_ = acceptor;
  return packetizer_->Listen(endpoint, &messenger_);
}

void CurveCPServerSocket::Close() {
  if (acceptor_)
    return;  // The listener socket is not closable.
  packetizer_->Close(key_);
}

int CurveCPServerSocket::Read(IOBuffer* buf,
                              int buf_len,
                              const CompletionCallback& callback) {
  return messenger_.Read(buf, buf_len, callback);
}

int CurveCPServerSocket::Write(IOBuffer* buf,
                               int buf_len,
                               const CompletionCallback& callback) {
  return messenger_.Write(buf, buf_len, callback);
}

bool CurveCPServerSocket::SetReceiveBufferSize(int32 size) {
  return true;
}

bool CurveCPServerSocket::SetSendBufferSize(int32 size) {
  return true;
}

void CurveCPServerSocket::OnAccept(ConnectionKey key) {
  DCHECK(acceptor_);

  CurveCPServerSocket* new_socket =
      new CurveCPServerSocket(key,
                              packetizer_,
                              net_log_.net_log(),
                              NetLog::Source());
  packetizer_->Open(key, new_socket->messenger());
  acceptor_->OnAccept(new_socket);
}

}  // namespace net
