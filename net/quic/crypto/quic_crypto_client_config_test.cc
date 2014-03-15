// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/quic_crypto_client_config.h"

#include "net/quic/crypto/proof_verifier.h"
#include "net/quic/quic_session_key.h"
#include "net/quic/test_tools/quic_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::string;

namespace net {
namespace test {

TEST(QuicCryptoClientConfigTest, CachedState_IsEmpty) {
  QuicCryptoClientConfig::CachedState state;
  EXPECT_TRUE(state.IsEmpty());
}

TEST(QuicCryptoClientConfigTest, CachedState_IsComplete) {
  QuicCryptoClientConfig::CachedState state;
  EXPECT_FALSE(state.IsComplete(QuicWallTime::FromUNIXSeconds(0)));
}

TEST(QuicCryptoClientConfigTest, CachedState_GenerationCounter) {
  QuicCryptoClientConfig::CachedState state;
  EXPECT_EQ(0u, state.generation_counter());
  state.SetProofInvalid();
  EXPECT_EQ(1u, state.generation_counter());
}

TEST(QuicCryptoClientConfigTest, CachedState_SetProofVerifyDetails) {
  QuicCryptoClientConfig::CachedState state;
  EXPECT_TRUE(state.proof_verify_details() == NULL);
  ProofVerifyDetails* details = new ProofVerifyDetails;
  state.SetProofVerifyDetails(details);
  EXPECT_EQ(details, state.proof_verify_details());
}

TEST(QuicCryptoClientConfigTest, CachedState_InitializeFrom) {
  QuicCryptoClientConfig::CachedState state;
  QuicCryptoClientConfig::CachedState other;
  state.set_source_address_token("TOKEN");
  // TODO(rch): Populate other fields of |state|.
  other.InitializeFrom(state);
  EXPECT_EQ(state.server_config(), other.server_config());
  EXPECT_EQ(state.source_address_token(), other.source_address_token());
  EXPECT_EQ(state.certs(), other.certs());
  EXPECT_EQ(1u, other.generation_counter());
}

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

TEST(QuicCryptoClientConfigTest, InitializeFrom) {
  QuicCryptoClientConfig config;
  QuicSessionKey canonical_key1("www.google.com", 80, false);
  QuicCryptoClientConfig::CachedState* state =
      config.LookupOrCreate(canonical_key1);
  // TODO(rch): Populate other fields of |state|.
  state->set_source_address_token("TOKEN");
  state->SetProofValid();

  QuicSessionKey other_key("mail.google.com", 80, false);
  config.InitializeFrom(other_key, canonical_key1, &config);
  QuicCryptoClientConfig::CachedState* other = config.LookupOrCreate(other_key);

  EXPECT_EQ(state->server_config(), other->server_config());
  EXPECT_EQ(state->source_address_token(), other->source_address_token());
  EXPECT_EQ(state->certs(), other->certs());
  EXPECT_EQ(1u, other->generation_counter());
}

}  // namespace test
}  // namespace net
