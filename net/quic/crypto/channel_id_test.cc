// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/channel_id.h"

#include "net/quic/test_tools/crypto_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using std::string;

namespace net {
namespace test {

// TODO(rtenneti): Enable testing of ChannelID.
TEST(ChannelIDTest, DISABLED_SignAndVerify) {
  scoped_ptr<ChannelIDSigner> signer(
      CryptoTestUtils::ChannelIDSignerForTesting());

  const string signed_data = "signed data";
  const string hostname = "foo.example.com";
  string key, signature;
  ASSERT_TRUE(signer->Sign(hostname, signed_data, &key, &signature));

  EXPECT_EQ(key, CryptoTestUtils::ChannelIDKeyForHostname(hostname));

  EXPECT_TRUE(ChannelIDVerifier::Verify(key, signed_data, signature));

  EXPECT_FALSE(ChannelIDVerifier::Verify("a" + key, signed_data, signature));
  EXPECT_FALSE(ChannelIDVerifier::Verify(key, "a" + signed_data, signature));

  scoped_ptr<char[]> bad_key(new char[key.size()]);
  memcpy(bad_key.get(), key.data(), key.size());
  bad_key[1] ^= 0x80;
  EXPECT_FALSE(ChannelIDVerifier::Verify(
      string(bad_key.get(), key.size()), signed_data, signature));

  scoped_ptr<char[]> bad_signature(new char[signature.size()]);
  memcpy(bad_signature.get(), signature.data(), signature.size());
  bad_signature[1] ^= 0x80;
  EXPECT_FALSE(ChannelIDVerifier::Verify(
      key, signed_data, string(bad_signature.get(), signature.size())));

  EXPECT_FALSE(ChannelIDVerifier::Verify(
      key, "wrong signed data", signature));
}

}  // namespace test
}  // namespace net
