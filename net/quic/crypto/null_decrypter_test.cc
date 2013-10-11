// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/null_decrypter.h"
#include "net/quic/test_tools/quic_test_utils.h"

using base::StringPiece;

namespace net {
namespace test {

class NullDecrypterTest : public ::testing::TestWithParam<bool> {
};

INSTANTIATE_TEST_CASE_P(HashLength, NullDecrypterTest,
                        ::testing::Values(false, true));
TEST_P(NullDecrypterTest, Decrypt) {
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
  const char* data =
      reinterpret_cast<const char*>(GetParam() ? expected_short : expected);
  size_t len = GetParam() ? arraysize(expected_short) : arraysize(expected);
  NullDecrypter decrypter(GetParam());
  scoped_ptr<QuicData> decrypted(
      decrypter.DecryptPacket(0, "hello world!", StringPiece(data, len)));
  ASSERT_TRUE(decrypted.get());
  EXPECT_EQ("goodbye!", decrypted->AsStringPiece());
}

TEST_P(NullDecrypterTest, BadHash) {
  unsigned char expected[] = {
    // fnv hash
    0x46, 0x11, 0xea, 0x5f,
    0xcf, 0x1d, 0x66, 0x5b,
    0xba, 0xf0, 0xbc, 0xfd,
    0x88, 0x79, 0xca, 0x37,
    // payload
    'g',  'o',  'o',  'd',
    'b',  'y',  'e',  '!',
  };
  unsigned char expected_short[] = {
    // fnv hash
    0x46, 0x11, 0xea, 0x5f,
    0xcf, 0x1d, 0x66, 0x5b,
    0xba, 0xf0, 0xbc, 0xfd,
    // payload
    'g',  'o',  'o',  'd',
    'b',  'y',  'e',  '!',
  };
  const char* data =
      reinterpret_cast<const char*>(GetParam() ? expected_short : expected);
  size_t len = GetParam() ? arraysize(expected_short) : arraysize(expected);
  NullDecrypter decrypter(GetParam());
  scoped_ptr<QuicData> decrypted(
      decrypter.DecryptPacket(0, "hello world!", StringPiece(data, len)));
  ASSERT_FALSE(decrypted.get());
}

TEST_P(NullDecrypterTest, ShortInput) {
  unsigned char expected[] = {
    // fnv hash (truncated)
    0x46, 0x11, 0xea, 0x5f,
    0xcf, 0x1d, 0x66, 0x5b,
    0x47, 0x91, 0xb2, 0x13,
    0xba, 0xf0, 0xbc,
  };
  unsigned char expected_short[] = {
    // fnv hash (truncated)
    0x46, 0x11, 0xea, 0x5f,
    0xcf, 0x1d, 0x66, 0x5b,
    0xba, 0xf0, 0xbc,
  };
  const char* data =
      reinterpret_cast<const char*>(GetParam() ? expected_short : expected);
  size_t len = GetParam() ? arraysize(expected_short) : arraysize(expected);
  NullDecrypter decrypter(GetParam());
  scoped_ptr<QuicData> decrypted(
      decrypter.DecryptPacket(0, "hello world!", StringPiece(data, len)));
  ASSERT_FALSE(decrypted.get());
}

}  // namespace test
}  // namespace net
