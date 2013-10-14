// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/null_encrypter.h"
#include "net/quic/test_tools/quic_test_utils.h"

using base::StringPiece;

namespace net {
namespace test {

class NullEncrypterTest : public ::testing::TestWithParam<bool> {
};

INSTANTIATE_TEST_CASE_P(HashLength, NullEncrypterTest,
                        ::testing::Values(false, true));

TEST_P(NullEncrypterTest, Encrypt) {
  unsigned char expected[] = {
    // fnv hash
    0xa0, 0x6f, 0x44, 0x8a,
    0x44, 0xf8, 0x18, 0x3b,
    0x47, 0x91, 0xb2, 0x13,
    0x6b, 0x09, 0xbb, 0xae,
    // payload
    'g',  'o',  'o',  'd',
    'b',  'y',  'e',  '!',
  };
  unsigned char expected_short[] = {
    // fnv hash
    0xa0, 0x6f, 0x44, 0x8a,
    0x44, 0xf8, 0x18, 0x3b,
    0x47, 0x91, 0xb2, 0x13,
    // payload
    'g',  'o',  'o',  'd',
    'b',  'y',  'e',  '!',
  };
  NullEncrypter encrypter(GetParam());
  scoped_ptr<QuicData> encrypted(
      encrypter.EncryptPacket(0, "hello world!", "goodbye!"));
  ASSERT_TRUE(encrypted.get());
  if (GetParam()) {
    test::CompareCharArraysWithHexError(
        "encrypted data", encrypted->data(), encrypted->length(),
        reinterpret_cast<const char*>(expected_short),
        arraysize(expected_short));
  } else {
    test::CompareCharArraysWithHexError(
        "encrypted data", encrypted->data(), encrypted->length(),
        reinterpret_cast<const char*>(expected), arraysize(expected));
  }
}

TEST_P(NullEncrypterTest, GetMaxPlaintextSize) {
  NullEncrypter encrypter(GetParam());
  if (GetParam()) {
    EXPECT_EQ(1000u, encrypter.GetMaxPlaintextSize(1012));
    EXPECT_EQ(100u, encrypter.GetMaxPlaintextSize(112));
    EXPECT_EQ(10u, encrypter.GetMaxPlaintextSize(22));
  } else {
    EXPECT_EQ(1000u, encrypter.GetMaxPlaintextSize(1016));
    EXPECT_EQ(100u, encrypter.GetMaxPlaintextSize(116));
    EXPECT_EQ(10u, encrypter.GetMaxPlaintextSize(26));
  }
}

TEST_P(NullEncrypterTest, GetCiphertextSize) {
  NullEncrypter encrypter(GetParam());
  if (GetParam()) {
    EXPECT_EQ(1012u, encrypter.GetCiphertextSize(1000));
    EXPECT_EQ(112u, encrypter.GetCiphertextSize(100));
    EXPECT_EQ(22u, encrypter.GetCiphertextSize(10));
  } else {
    EXPECT_EQ(1016u, encrypter.GetCiphertextSize(1000));
    EXPECT_EQ(116u, encrypter.GetCiphertextSize(100));
    EXPECT_EQ(26u, encrypter.GetCiphertextSize(10));
  }
}

}  // namespace test
}  // namespace net
