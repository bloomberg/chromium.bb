// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/base/sys_addrinfo.h"
#include "net/curvecp/curvecp_client_socket.h"
#include "net/curvecp/messenger.h"

namespace net {

CurveCPClientSocket::CurveCPClientSocket(const AddressList& addresses,
                                         net::NetLog* net_log,
                                         const net::NetLog::Source& source)
    : addresses_(addresses),
      net_log_(BoundNetLog::Make(net_log, NetLog::SOURCE_SOCKET)),
      messenger_(&packetizer_) {
}

CurveCPClientSocket::~CurveCPClientSocket() {
}

int CurveCPClientSocket::Connect(OldCompletionCallback* callback) {
  return packetizer_.Connect(addresses_, &messenger_, callback);
}

void CurveCPClientSocket::Disconnect() {
  // TODO(mbelshe): DCHECK that we're connected.
  // Record the ConnectionKey so that we can disconnect it properly.
  // Do we need a close() on the messenger?
  // packetizer_.Close();
}

bool CurveCPClientSocket::IsConnected() const {
  // TODO(mbelshe): return packetizer_.IsConnected();
  return false;
}

bool CurveCPClientSocket::IsConnectedAndIdle() const {
  // TODO(mbelshe): return packetizer_.IsConnectedAndIdle();
  return false;
}

int CurveCPClientSocket::GetPeerAddress(AddressList* address) const {
  IPEndPoint endpoint;
  int rv = packetizer_.GetPeerAddress(&endpoint);
  if (rv < 0)
    return rv;
  struct sockaddr_storage sockaddr;
  size_t sockaddr_length = sizeof(sockaddr);
  bool success = endpoint.ToSockAddr(
      reinterpret_cast<struct sockaddr*>(&sockaddr), &sockaddr_length);
  if (!success)
    return ERR_FAILED;
  struct addrinfo ai;
  memset(&ai, 0, sizeof(ai));
  memcpy(&ai.ai_addr, &sockaddr, sockaddr_length);
  ai.ai_addrlen = sockaddr_length;
  *address = AddressList::CreateByCopying(&ai);
  return OK;
}

int CurveCPClientSocket::GetLocalAddress(IPEndPoint* address) const {
  NOTIMPLEMENTED();
  return ERR_FAILED;
}

const BoundNetLog& CurveCPClientSocket::NetLog() const {
  return net_log_;
}

void CurveCPClientSocket::SetSubresourceSpeculation() {
  // This is ridiculous.
}

void CurveCPClientSocket::SetOmniboxSpeculation() {
  // This is ridiculous.
}

bool CurveCPClientSocket::WasEverUsed() const {
  // This is ridiculous.
  return true;
}

bool CurveCPClientSocket::UsingTCPFastOpen() const {
  // This is ridiculous.
  return false;
}

int64 CurveCPClientSocket::NumBytesRead() const {
  return -1;
}

base::TimeDelta CurveCPClientSocket::GetConnectTimeMicros() const {
  return base::TimeDelta::FromMicroseconds(-1);
}

int CurveCPClientSocket::Read(IOBuffer* buf,
                              int buf_len,
                              OldCompletionCallback* callback) {
  return messenger_.Read(buf, buf_len, callback);
}

int CurveCPClientSocket::Write(IOBuffer* buf,
                               int buf_len,
                               OldCompletionCallback* callback) {
  return messenger_.Write(buf, buf_len, callback);
}

bool CurveCPClientSocket::SetReceiveBufferSize(int32 size) {
  return true;
}

bool CurveCPClientSocket::SetSendBufferSize(int32 size) {
  return true;
}

}  // namespace net
