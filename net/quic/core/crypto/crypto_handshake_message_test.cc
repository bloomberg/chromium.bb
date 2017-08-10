// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/core/crypto/crypto_handshake_message.h"

#include "net/quic/core/crypto/crypto_handshake.h"
#include "net/quic/core/crypto/crypto_protocol.h"
#include "net/quic/platform/api/quic_endian.h"
#include "net/quic/platform/api/quic_test.h"

namespace net {
namespace test {
namespace {

class CryptoHandshakeMessageTest : public QuicTestWithParam<Perspective> {};

INSTANTIATE_TEST_CASE_P(Perspective,
                        CryptoHandshakeMessageTest,
                        ::testing::ValuesIn({Perspective::IS_CLIENT,
                                             Perspective::IS_SERVER}));

TEST_P(CryptoHandshakeMessageTest, DebugString) {
  const char* str = "SHLO<\n>";

  CryptoHandshakeMessage message;
  message.set_tag(kSHLO);
  EXPECT_EQ(str, message.DebugString(GetParam()));

  // Test copy
  CryptoHandshakeMessage message2(message);
  EXPECT_EQ(str, message2.DebugString(GetParam()));

  // Test move
  CryptoHandshakeMessage message3(std::move(message));
  EXPECT_EQ(str, message3.DebugString(GetParam()));

  // Test assign
  CryptoHandshakeMessage message4 = message3;
  EXPECT_EQ(str, message4.DebugString(GetParam()));

  // Test move-assign
  CryptoHandshakeMessage message5 = std::move(message3);
  EXPECT_EQ(str, message5.DebugString(GetParam()));
}

TEST_P(CryptoHandshakeMessageTest, DebugStringWithUintVector) {
  const char* str =
      "REJ <\n  RREJ: "
      "SOURCE_ADDRESS_TOKEN_DIFFERENT_IP_ADDRESS_FAILURE,"
      "CLIENT_NONCE_NOT_UNIQUE_FAILURE\n>";

  CryptoHandshakeMessage message;
  message.set_tag(kREJ);
  std::vector<uint32_t> reasons = {
      SOURCE_ADDRESS_TOKEN_DIFFERENT_IP_ADDRESS_FAILURE,
      CLIENT_NONCE_NOT_UNIQUE_FAILURE};
  message.SetVector(kRREJ, reasons);
  EXPECT_EQ(str, message.DebugString(GetParam()));

  // Test copy
  CryptoHandshakeMessage message2(message);
  EXPECT_EQ(str, message2.DebugString(GetParam()));

  // Test move
  CryptoHandshakeMessage message3(std::move(message));
  EXPECT_EQ(str, message3.DebugString(GetParam()));

  // Test assign
  CryptoHandshakeMessage message4 = message3;
  EXPECT_EQ(str, message4.DebugString(GetParam()));

  // Test move-assign
  CryptoHandshakeMessage message5 = std::move(message3);
  EXPECT_EQ(str, message5.DebugString(GetParam()));
}

TEST_P(CryptoHandshakeMessageTest, DebugStringWithTagVector) {
  const char* str = "CHLO<\n  COPT: 'TBBR','PAD ','BYTE'\n>";

  CryptoHandshakeMessage message;
  message.set_tag(kCHLO);
  message.SetVector(kCOPT, QuicTagVector{kTBBR, kPAD, kBYTE});
  EXPECT_EQ(str, message.DebugString(GetParam()));

  // Test copy
  CryptoHandshakeMessage message2(message);
  EXPECT_EQ(str, message2.DebugString(GetParam()));

  // Test move
  CryptoHandshakeMessage message3(std::move(message));
  EXPECT_EQ(str, message3.DebugString(GetParam()));

  // Test assign
  CryptoHandshakeMessage message4 = message3;
  EXPECT_EQ(str, message4.DebugString(GetParam()));

  // Test move-assign
  CryptoHandshakeMessage message5 = std::move(message3);
  EXPECT_EQ(str, message5.DebugString(GetParam()));
}

TEST_P(CryptoHandshakeMessageTest, ServerDesignatedConnectionId) {
  const char* str = "SREJ<\n  RCID: 18364758544493064720\n>";

  CryptoHandshakeMessage message;
  message.set_tag(kSREJ);
  message.SetValue(kRCID,
                   QuicEndian::NetToHost64(UINT64_C(18364758544493064720)));
  EXPECT_EQ(str, message.DebugString(GetParam()));

  // Test copy
  CryptoHandshakeMessage message2(message);
  EXPECT_EQ(str, message2.DebugString(GetParam()));

  // Test move
  CryptoHandshakeMessage message3(std::move(message));
  EXPECT_EQ(str, message3.DebugString(GetParam()));

  // Test assign
  CryptoHandshakeMessage message4 = message3;
  EXPECT_EQ(str, message4.DebugString(GetParam()));

  // Test move-assign
  CryptoHandshakeMessage message5 = std::move(message3);
  EXPECT_EQ(str, message5.DebugString(GetParam()));
}

TEST_P(CryptoHandshakeMessageTest, HasStringPiece) {
  CryptoHandshakeMessage message;
  EXPECT_FALSE(message.HasStringPiece(kRCID));
  message.SetStringPiece(kRCID, "foo");
  EXPECT_TRUE(message.HasStringPiece(kRCID));
}

}  // namespace
}  // namespace test
}  // namespace net
