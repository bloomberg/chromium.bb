// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gcm_driver/crypto/p256_key_util.h"

#include <stddef.h>

#include <set>

#include "base/base64.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace gcm {

namespace {

// A P-256 point in uncompressed form consists of 0x04 (to denote that the point
// is uncompressed per SEC1 2.3.3) followed by two, 32-byte field elements.
const size_t kUncompressedPointBytes = 1 + 2 * 32;

// Precomputed private/public key-pair. Keys are stored on disk, so previously
// created values must continue to be usable for computing shared secrets.
const char kBobPrivateKey[] =
    "MIGwMBsGCiqGSIb3DQEMAQMwDQQIyCB6/yia3wACAQEEgZCp4yoEpn9ZITGy7mt2+zqI1IFgHy"
    "1RglXKiPdy+Ue5eubZ3Xe/pz/5yQWvysXLaTT+/LeuAoq2gwKK2AINTWglIEMCmcveERa0gZDW"
    "avhYEYtEynNLVop4bHlii5DcUY1/e9DY5Dxh7b2dYdWI2Ev+529KughVKK8qFu12wn3cSSlfXh"
    "9Hb8Nh/4yP0jg6k5k=";
const char kBobPublicKeyX509[] =
    "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAEVVJqpW6OqkeXNhDBZjweZb+he+5Iyca7vwIYU3"
    "IdDHup4xG3A1Ih03GQE6jBz+0g9x/Rf/J2tgEg2DWl2TxD/Q==";
const char kBobPublicKey[] =
    "BFVSaqVujqpHlzYQwWY8HmW/oXvuSMnGu78CGFNyHQx7qeMRtwNSIdNxkBOowc/tIPcf0X/ydr"
    "YBINg1pdk8Q/0=";

const char kCarolPrivateKey[] =
    "MIGwMBsGCiqGSIb3DQEMAQMwDQQIOyIGhczk5TECAQEEgZCQO0tXSd0dCxuKCcmU462i+VNwJz"
    "J4KT/C32F6K3edQnZ2J750g6nMVtkoK9TF23UcEIVB0Lo7FG4T5WF03wjC4A+5FC/1mYzsWFHO"
    "6AugLhum5psqX3fq6UmgYoir9dJsI7Rmmn1JH8Gtw6KJHMncPi1lGriLZqzcrw+oULVf6dcnH1"
    "z9F39GuYob5+sY7ak=";
const char kCarolPublicKeyX509[] =
    "MFkwEwYHKoZIzj0CAQYIKoZIzj0DAQcDQgAETqNDkapziFRUvEXfP9zEpwJi+kYh/+JbV5BZDD"
    "QKS9kmXvKz+ahuKVmpYNkDI0ZdYM1KkNMUxd4/hprQ1L9sEA==";
const char kCarolPublicKey[] =
    "BE6jQ5Gqc4hUVLxF3z/cxKcCYvpGIf/iW1eQWQw0CkvZJl7ys/mobilZqWDZAyNGXWDNSpDTFM"
    "XeP4aa0NS/bBA=";

// The shared secret between Bob and Carol.
const char kBobCarolSharedSecret[] =
    "epd8m4CulXi284tsHYbGIZJg73xnxoNvlP/qLyqkA30=";

TEST(P256KeyUtilTest, UniqueKeyPairGeneration) {
  // Canary for determining that no key repetitions are found in few iterations.
  std::set<std::string> seen_private_keys;
  std::set<std::string> seen_public_keys_x509;
  std::set<std::string> seen_public_keys;

  for (int iteration = 0; iteration < 10; ++iteration) {
    SCOPED_TRACE(iteration);

    std::string private_key, public_key_x509, public_key;
    ASSERT_TRUE(CreateP256KeyPair(&private_key, &public_key_x509, &public_key));

    EXPECT_NE(private_key, public_key);
    EXPECT_GT(private_key.size(), 0u);
    EXPECT_GT(public_key_x509.size(), 0u);
    EXPECT_EQ(public_key.size(), kUncompressedPointBytes);

    EXPECT_EQ(0u, seen_private_keys.count(private_key));
    EXPECT_EQ(0u, seen_public_keys_x509.count(public_key_x509));
    EXPECT_EQ(0u, seen_public_keys.count(public_key));

    seen_private_keys.insert(private_key);
    seen_public_keys_x509.insert(public_key_x509);
    seen_public_keys.insert(public_key);
  }
}

TEST(P256KeyUtilTest, SharedSecretCalculation) {
  std::string bob_private_key, bob_public_key_x509, bob_public_key;
  std::string alice_private_key, alice_public_key_x509, alice_public_key;

  ASSERT_TRUE(CreateP256KeyPair(
      &bob_private_key, &bob_public_key_x509, &bob_public_key));
  ASSERT_TRUE(CreateP256KeyPair(
      &alice_private_key, &alice_public_key_x509, &alice_public_key));
  ASSERT_NE(bob_private_key, alice_private_key);

  std::string bob_shared_secret, alice_shared_secret;
  ASSERT_TRUE(ComputeSharedP256Secret(bob_private_key, bob_public_key_x509,
                                      alice_public_key, &bob_shared_secret));
  ASSERT_TRUE(ComputeSharedP256Secret(alice_private_key, alice_public_key_x509,
                                      bob_public_key, &alice_shared_secret));

  EXPECT_GT(bob_shared_secret.size(), 0u);
  EXPECT_EQ(bob_shared_secret, alice_shared_secret);

  std::string unused_shared_secret;

  // Empty and too short peer public values should be considered invalid.
  ASSERT_FALSE(ComputeSharedP256Secret(bob_private_key, bob_public_key_x509,
                                       "", &unused_shared_secret));
  ASSERT_FALSE(ComputeSharedP256Secret(bob_private_key, bob_public_key_x509,
                                       bob_public_key.substr(1),
                                       &unused_shared_secret));

}

TEST(P256KeyUtilTest, SharedSecretWithPreExistingKey) {
  std::string bob_private_key, bob_public_key_x509, bob_public_key;
  std::string alice_private_key, alice_public_key_x509, alice_public_key;

  ASSERT_TRUE(base::Base64Decode(kBobPrivateKey, &bob_private_key));
  ASSERT_TRUE(base::Base64Decode(kBobPublicKeyX509, &bob_public_key_x509));
  ASSERT_TRUE(base::Base64Decode(kBobPublicKey, &bob_public_key));

  // First verify against a newly created, ephemeral key-pair.
  ASSERT_TRUE(CreateP256KeyPair(
      &alice_private_key, &alice_public_key_x509, &alice_public_key));

  std::string bob_shared_secret, alice_shared_secret;
  ASSERT_TRUE(ComputeSharedP256Secret(bob_private_key, bob_public_key_x509,
                                      alice_public_key, &bob_shared_secret));
  ASSERT_TRUE(ComputeSharedP256Secret(alice_private_key, alice_public_key_x509,
                                      bob_public_key, &alice_shared_secret));

  EXPECT_GT(bob_shared_secret.size(), 0u);
  EXPECT_EQ(bob_shared_secret, alice_shared_secret);

  std::string carol_private_key, carol_public_key_x509, carol_public_key;

  ASSERT_TRUE(base::Base64Decode(kCarolPrivateKey, &carol_private_key));
  ASSERT_TRUE(base::Base64Decode(kCarolPublicKeyX509, &carol_public_key_x509));
  ASSERT_TRUE(base::Base64Decode(kCarolPublicKey, &carol_public_key));

  bob_shared_secret.clear();
  std::string carol_shared_secret;

  // Then verify against another stored key-pair and shared secret.
  ASSERT_TRUE(ComputeSharedP256Secret(bob_private_key, bob_public_key_x509,
                                      carol_public_key, &bob_shared_secret));
  ASSERT_TRUE(ComputeSharedP256Secret(carol_private_key, carol_public_key_x509,
                                      bob_public_key, &carol_shared_secret));

  EXPECT_GT(carol_shared_secret.size(), 0u);
  EXPECT_EQ(carol_shared_secret, bob_shared_secret);

  std::string bob_carol_shared_secret;
  ASSERT_TRUE(base::Base64Decode(
      kBobCarolSharedSecret, &bob_carol_shared_secret));

  EXPECT_EQ(carol_shared_secret, bob_carol_shared_secret);
}

}  // namespace

}  // namespace gcm
