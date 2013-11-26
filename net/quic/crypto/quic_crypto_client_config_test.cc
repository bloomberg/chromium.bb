// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/quic_crypto_client_config.h"

#include "net/quic/test_tools/quic_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::string;

namespace net {
namespace test {

TEST(QuicCryptoClientConfigTest, InchoateChlo) {
  QuicCryptoClientConfig::CachedState state;
  QuicCryptoClientConfig config;
  QuicCryptoNegotiatedParameters params;
  CryptoHandshakeMessage msg;
  config.FillInchoateClientHello("www.google.com", QuicVersionMax(), &state,
                                 &params, &msg);

  QuicTag cver;
  EXPECT_EQ(QUIC_NO_ERROR, msg.GetUint32(kVER, &cver));
  EXPECT_EQ(QuicVersionToQuicTag(QuicVersionMax()), cver);

  // TODO(rch): Remove once we remove QUIC_VERSION_12.
  uint16 vers;
  EXPECT_EQ(QUIC_NO_ERROR, msg.GetUint16(kVERS, &vers));
  EXPECT_EQ(0u, vers);
}

TEST(QuicCryptoClientConfigTest, ProcessServerDowngradeAttack) {
  QuicVersionVector supported_versions = QuicSupportedVersions();
  if (supported_versions.size() == 1) {
    // No downgrade attack is possible if the client only supports one version.
    return;
  }
  QuicTagVector supported_version_tags;
  for (size_t i = supported_versions.size(); i > 0; --i) {
    supported_version_tags.push_back(
        QuicVersionToQuicTag(supported_versions[i - 1]));
  }
  CryptoHandshakeMessage msg;
  msg.set_tag(kSHLO);
  msg.SetVector(kVER, supported_version_tags);

  QuicCryptoClientConfig::CachedState cached;
  QuicCryptoNegotiatedParameters out_params;
  string error;
  QuicCryptoClientConfig config;
  EXPECT_EQ(QUIC_VERSION_NEGOTIATION_MISMATCH,
            config.ProcessServerHello(msg, 0, supported_versions,
                                      &cached, &out_params, &error));
  EXPECT_EQ("Downgrade attack detected", error);
}

}  // namespace test
}  // namespace net
