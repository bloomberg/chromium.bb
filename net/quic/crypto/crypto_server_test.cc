// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_number_conversions.h"
#include "net/quic/crypto/crypto_server_config.h"
#include "net/quic/crypto/crypto_utils.h"
#include "net/quic/crypto/quic_random.h"
#include "net/quic/test_tools/crypto_test_utils.h"
#include "net/quic/test_tools/mock_clock.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::StringPiece;
using std::string;

namespace net {
namespace test {

class CryptoServerTest : public ::testing::Test {
 public:
  CryptoServerTest()
      : rand_(QuicRandom::GetInstance()),
        config_(QuicCryptoServerConfig::TESTING, rand_),
        addr_(ParseIPLiteralToNumber("192.0.2.33", &ip_) ?
              ip_ : IPAddressNumber(), 1) {
  }

  virtual void SetUp() {
    scoped_ptr<CryptoHandshakeMessage> msg(
        config_.AddDefaultConfig(rand_, &clock_,
        QuicCryptoServerConfig::ConfigOptions()));

    StringPiece orbit;
    CHECK(msg->GetStringPiece(kORBT, &orbit));
    CHECK_EQ(sizeof(orbit_), orbit.size());
    memcpy(orbit_, orbit.data(), orbit.size());
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

  string GenerateNonce() {
    string nonce;
    CryptoUtils::GenerateNonce(
        clock_.WallNow(), rand_,
        StringPiece(reinterpret_cast<const char*>(orbit_), sizeof(orbit_)),
        &nonce);
    return nonce;
  }

 protected:
  QuicRandom* const rand_;
  MockClock clock_;
  QuicCryptoServerConfig config_;
  QuicCryptoNegotiatedParameters params_;
  CryptoHandshakeMessage out_;
  IPAddressNumber ip_;
  IPEndPoint addr_;
  uint8 orbit_[kOrbitSize];
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

TEST_F(CryptoServerTest, ReplayProtection) {
  // This tests that disabling replay protection works.

  char public_value[32];
  memset(public_value, 42, sizeof(public_value));

  const string nonce_str = GenerateNonce();
  const string nonce("#" + base::HexEncode(nonce_str.data(),
                                           nonce_str.size()));
  const string pub("#" + base::HexEncode(public_value, sizeof(public_value)));

  CryptoHandshakeMessage msg = CryptoTestUtils::Message(
      "CHLO",
      "AEAD", "AESG",
      "KEXS", "C255",
      "PUBS", pub.c_str(),
      "NONC", nonce.c_str(),
      "$padding", static_cast<int>(kClientHelloMinimumSize),
      NULL);
  ShouldSucceed(msg);
  // The message should be rejected because the source-address token is missing.
  ASSERT_EQ(kREJ, out_.tag());

  StringPiece srct;
  ASSERT_TRUE(out_.GetStringPiece(kSourceAddressTokenTag, &srct));
  const string srct_hex = "#" + base::HexEncode(srct.data(), srct.size());

  StringPiece scfg;
  ASSERT_TRUE(out_.GetStringPiece(kSCFG, &scfg));
  scoped_ptr<CryptoHandshakeMessage> server_config(
      CryptoFramer::ParseMessage(scfg));

  StringPiece scid;
  ASSERT_TRUE(server_config->GetStringPiece(kSCID, &scid));
  const string scid_hex("#" + base::HexEncode(scid.data(), scid.size()));

  msg = CryptoTestUtils::Message(
      "CHLO",
      "AEAD", "AESG",
      "KEXS", "C255",
      "SCID", scid_hex.c_str(),
      "#004b5453", srct_hex.c_str(),
      "PUBS", pub.c_str(),
      "NONC", nonce.c_str(),
      "$padding", static_cast<int>(kClientHelloMinimumSize),
      NULL);
  ShouldSucceed(msg);
  // The message should be rejected because the strike-register is still
  // quiescent.
  ASSERT_EQ(kREJ, out_.tag());

  config_.set_replay_protection(false);

  ShouldSucceed(msg);
  // The message should be accepted now.
  ASSERT_EQ(kSHLO, out_.tag());

  ShouldSucceed(msg);
  // The message should accepted twice when replay protection is off.
  ASSERT_EQ(kSHLO, out_.tag());
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
