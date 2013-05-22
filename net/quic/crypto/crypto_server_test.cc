// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/crypto_server_config.h"
#include "net/quic/crypto/quic_random.h"
#include "net/quic/test_tools/crypto_test_utils.h"
#include "net/quic/test_tools/mock_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::string;

namespace net {
namespace test {

class CryptoServerTest : public ::testing::Test {
 public:
  CryptoServerTest()
      : rand_(QuicRandom::GetInstance()),
        config_(QuicCryptoServerConfig::TESTING),
        addr_(ParseIPLiteralToNumber("192.0.2.33", &ip_) ?
              ip_ : IPAddressNumber(), 1) {
  }

  virtual void SetUp() {
    scoped_ptr<CryptoHandshakeMessage> msg(
        config_.AddDefaultConfig(rand_, &clock_, 0));
  }

  void ShouldSucceed(const CryptoHandshakeMessage& message) {
    string error_details;
    QuicErrorCode error = config_.ProcessClientHello(
        message, 1 /* GUID */, addr_, &clock_,
        rand_, &params_, &out_, &error_details);

    ASSERT_EQ(error, QUIC_NO_ERROR)
        << "Message failed with error " << error_details << ": "
        << message.DebugString();
  }

  void ShouldFailMentioning(const char* error_substr,
                            const CryptoHandshakeMessage& message) {
    string error_details;
    QuicErrorCode error = config_.ProcessClientHello(
        message, 1 /* GUID */, addr_, &clock_,
        rand_, &params_, &out_, &error_details);

    ASSERT_NE(error, QUIC_NO_ERROR)
        << "Message didn't fail: " << message.DebugString();

    EXPECT_TRUE(error_details.find(error_substr) != string::npos)
        << error_substr << " not in " << error_details;
  }

  CryptoHandshakeMessage InchoateClientHello(const char* message_tag, ...) {
    va_list ap;
    va_start(ap, message_tag);

    CryptoHandshakeMessage message =
        CryptoTestUtils::BuildMessage(message_tag, ap);
    va_end(ap);

    message.SetStringPiece(kPAD, string(kClientHelloMinimumSize, '-'));
    return message;
  }

 private:
  QuicRandom* const rand_;
  MockClock clock_;
  QuicCryptoServerConfig config_;
  QuicCryptoNegotiatedParameters params_;
  CryptoHandshakeMessage out_;
  IPAddressNumber ip_;
  IPEndPoint addr_;
};

TEST_F(CryptoServerTest, BadSNI) {
  static const char* kBadSNIs[] = {
    "",
    "foo",
    "#00",
    "#ff00",
    "127.0.0.1",
    "ffee::1",
  };

  for (size_t i = 0; i < arraysize(kBadSNIs); i++) {
    ShouldFailMentioning("SNI", InchoateClientHello(
        "CHLO",
        "SNI", kBadSNIs[i],
        NULL));
  }
}

TEST_F(CryptoServerTest, TooSmall) {
  ShouldFailMentioning("too small", CryptoTestUtils::Message(
        "CHLO",
        NULL));
}

TEST_F(CryptoServerTest, BadSourceAddressToken) {
  // Invalid source-address tokens should be ignored.
  static const char* kBadSourceAddressTokens[] = {
    "",
    "foo",
    "#0000",
    "#0000000000000000000000000000000000000000",
  };

  for (size_t i = 0; i < arraysize(kBadSourceAddressTokens); i++) {
    ShouldSucceed(InchoateClientHello(
        "CHLO",
        "STK", kBadSourceAddressTokens[i],
        NULL));
  }
}

TEST_F(CryptoServerTest, BadClientNonce) {
  // Invalid nonces should be ignored.
  static const char* kBadNonces[] = {
    "",
    "#0000",
    "#0000000000000000000000000000000000000000",
  };

  for (size_t i = 0; i < arraysize(kBadNonces); i++) {
    ShouldSucceed(InchoateClientHello(
        "CHLO",
        "NONC", kBadNonces[i],
        NULL));
  }
}

class CryptoServerTestNoConfig : public CryptoServerTest {
 public:
  virtual void SetUp() {
    // Deliberately don't add a config so that we can test this situation.
  }
};

TEST_F(CryptoServerTestNoConfig, DontCrash) {
    ShouldFailMentioning("No config", InchoateClientHello(
        "CHLO",
        NULL));
}

}  // namespace test
}  // namespace net
