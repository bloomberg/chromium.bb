// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_TEST_COMMON_H_
#define BLIMP_NET_TEST_COMMON_H_

#include <string>

#include "blimp/net/blimp_message_processor.h"
#include "net/socket/stream_socket.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace net {
class GrowableIOBuffer;
}  // namespace net

namespace blimp {

// Checks if the contents of a buffer are an exact match with std::string.
// Using this matcher for inequality checks will result in undefined behavior,
// due to IOBuffer's lack of a size field.
//
// arg (type: IOBuffer*) The buffer to check.
// data (type: std::string) The string to compare with |arg|.
MATCHER_P(BufferEquals, expected, "") {
  return expected == std::string(arg->data(), expected.size());
}

// GMock action that writes data from a string to an IOBuffer.
//
//   buf_idx (template parameter 0): 0-based index of the IOBuffer arg.
//   str: the string containing data to be written to the IOBuffer.
ACTION_TEMPLATE(FillBufferFromString,
                HAS_1_TEMPLATE_PARAMS(int, buf_idx),
                AND_1_VALUE_PARAMS(str)) {
  memcpy(testing::get<buf_idx>(args)->data(), str.data(), str.size());
}

// Formats a string-based representation of a BlimpMessage header.
std::string EncodeHeader(size_t size);

class MockStreamSocket : public net::StreamSocket {
 public:
  MockStreamSocket();
  virtual ~MockStreamSocket();

  MOCK_METHOD3(Read, int(net::IOBuffer*, int, const net::CompletionCallback&));
  MOCK_METHOD3(Write, int(net::IOBuffer*, int, const net::CompletionCallback&));
  MOCK_METHOD1(SetReceiveBufferSize, int(int32));
  MOCK_METHOD1(SetSendBufferSize, int(int32));
  MOCK_METHOD1(Connect, int(const net::CompletionCallback&));
  MOCK_METHOD0(Disconnect, void());
  MOCK_CONST_METHOD0(IsConnected, bool());
  MOCK_CONST_METHOD0(IsConnectedAndIdle, bool());
  MOCK_CONST_METHOD1(GetPeerAddress, int(net::IPEndPoint*));
  MOCK_CONST_METHOD1(GetLocalAddress, int(net::IPEndPoint*));
  MOCK_CONST_METHOD0(NetLog, const net::BoundNetLog&());
  MOCK_METHOD0(SetSubresourceSpeculation, void());
  MOCK_METHOD0(SetOmniboxSpeculation, void());
  MOCK_CONST_METHOD0(WasEverUsed, bool());
  MOCK_CONST_METHOD0(UsingTCPFastOpen, bool());
  MOCK_CONST_METHOD0(NumBytesRead, int64());
  MOCK_CONST_METHOD0(GetConnectTimeMicros, base::TimeDelta());
  MOCK_CONST_METHOD0(WasNpnNegotiated, bool());
  MOCK_CONST_METHOD0(GetNegotiatedProtocol, net::NextProto());
  MOCK_METHOD1(GetSSLInfo, bool(net::SSLInfo*));
  MOCK_CONST_METHOD1(GetConnectionAttempts, void(net::ConnectionAttempts*));
  MOCK_METHOD0(ClearConnectionAttempts, void());
  MOCK_METHOD1(AddConnectionAttempts, void(const net::ConnectionAttempts&));
  MOCK_CONST_METHOD0(GetTotalReceivedBytes, int64_t());
};

class MockBlimpMessageProcessor : public BlimpMessageProcessor {
 public:
  MockBlimpMessageProcessor();

  ~MockBlimpMessageProcessor() override;

  // Adapts calls from ProcessMessage to MockableProcessMessage by
  // unboxing the |message| scoped_ptr for GMock compatibility.
  void ProcessMessage(scoped_ptr<BlimpMessage> message,
                      const net::CompletionCallback& callback) override;

  MOCK_METHOD2(MockableProcessMessage,
               void(const BlimpMessage& message,
                    const net::CompletionCallback& callback));
};

// Returns true if |buf| has a prefix of |str|.
// Behavior is undefined if len(buf) < len(str).
bool BufferStartsWith(net::GrowableIOBuffer* buf, const std::string& str);

}  // namespace blimp

#endif  // BLIMP_NET_TEST_COMMON_H_
