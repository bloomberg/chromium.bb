// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_NET_MOCK_STREAM_SOCKET_H_
#define CHROMECAST_NET_MOCK_STREAM_SOCKET_H_

#include <stdint.h>

#include "base/macros.h"
#include "net/log/net_log_with_source.h"
#include "net/socket/stream_socket.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace chromecast {

// Google Mock implementation of StreamSocket.
class MockStreamSocket : public net::StreamSocket {
 public:
  MockStreamSocket();
  ~MockStreamSocket() override;
  MOCK_METHOD3(Read, int(net::IOBuffer*, int, const net::CompletionCallback&));
  MOCK_METHOD3(Write, int(net::IOBuffer*, int, const net::CompletionCallback&));
  MOCK_METHOD1(SetReceiveBufferSize, int(int32_t));
  MOCK_METHOD1(SetSendBufferSize, int(int32_t));
  MOCK_METHOD1(Connect, int(const net::CompletionCallback&));
  MOCK_METHOD0(Disconnect, void());
  MOCK_CONST_METHOD0(IsConnected, bool());
  MOCK_CONST_METHOD0(IsConnectedAndIdle, bool());
  MOCK_CONST_METHOD1(GetPeerAddress, int(net::IPEndPoint*));
  MOCK_CONST_METHOD1(GetLocalAddress, int(net::IPEndPoint*));
  MOCK_CONST_METHOD0(NetLog, const net::NetLogWithSource&());
  MOCK_METHOD0(SetSubresourceSpeculation, void());
  MOCK_METHOD0(SetOmniboxSpeculation, void());
  MOCK_CONST_METHOD0(WasEverUsed, bool());
  MOCK_CONST_METHOD0(UsingTCPFastOpen, bool());
  MOCK_CONST_METHOD0(WasAlpnNegotiated, bool());
  MOCK_CONST_METHOD0(GetNegotiatedProtocol, net::NextProto());
  MOCK_METHOD1(GetSSLInfo, bool(net::SSLInfo*));
  MOCK_CONST_METHOD1(GetConnectionAttempts, void(net::ConnectionAttempts*));
  MOCK_METHOD0(ClearConnectionAttempts, void());
  MOCK_METHOD1(AddConnectionAttempts, void(const net::ConnectionAttempts&));
  MOCK_CONST_METHOD0(GetTotalReceivedBytes, int64_t());

 private:
  net::NetLogWithSource net_log_;

  DISALLOW_COPY_AND_ASSIGN(MockStreamSocket);
};

}  // namespace chromecast

#endif  // CHROMECAST_NET_MOCK_STREAM_SOCKET_H_
