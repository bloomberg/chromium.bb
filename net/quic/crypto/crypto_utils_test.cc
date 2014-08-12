// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/crypto/crypto_utils.h"

#include "net/quic/test_tools/quic_test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace net {
namespace test {
namespace {

TEST(CryptoUtilsTest, IsValidSNI) {
  // IP as SNI.
  EXPECT_FALSE(CryptoUtils::IsValidSNI("192.168.0.1"));
  // SNI without any dot.
  EXPECT_FALSE(CryptoUtils::IsValidSNI("somedomain"));
  // Invalid RFC2396 hostname
  // TODO(rtenneti): Support RFC2396 hostname.
  // EXPECT_FALSE(CryptoUtils::IsValidSNI("some_domain.com"));
  // An empty string must be invalid otherwise the QUIC client will try sending
  // it.
  EXPECT_FALSE(CryptoUtils::IsValidSNI(""));

  // Valid SNI
  EXPECT_TRUE(CryptoUtils::IsValidSNI("test.google.com"));
}

TEST(CryptoUtilsTest, NormalizeHostname) {
  struct {
    const char *input, *expected;
  } tests[] = {
    { "www.google.com", "www.google.com", },
    { "WWW.GOOGLE.COM", "www.google.com", },
    { "www.google.com.", "www.google.com", },
    { "www.google.COM.", "www.google.com", },
    { "www.google.com..", "www.google.com", },
    { "www.google.com........", "www.google.com", },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(tests); ++i) {
    EXPECT_EQ(std::string(tests[i].expected),
              CryptoUtils::NormalizeHostname(tests[i].input));
  }
}

TEST(CryptoUtilsTest, TestExportKeyingMaterial) {
  const struct TestVector {
    // Input (strings of hexadecimal digits):
    const char* subkey_secret;
    const char* label;
    const char* context;
    size_t result_len;

    // Expected output (string of hexadecimal digits):
    const char* expected;  // Null if it should fail.
  } test_vector[] = {
    // Try a typical input
    { "4823c1189ecc40fce888fbb4cf9ae6254f19ba12e6d9af54788f195a6f509ca3",
      "e934f78d7a71dd85420fceeb8cea0317",
      "b8d766b5d3c8aba0009c7ed3de553eba53b4de1030ea91383dcdf724cd8b7217",
      32,
      "a9979da0d5f1c1387d7cbe68f5c4163ddb445a03c4ad6ee72cb49d56726d679e"
    },
    // Don't let the label contain nulls
    { "14fe51e082ffee7d1b4d8d4ab41f8c55",
      "3132333435363700",
      "58585858585858585858585858585858",
      16,
      NULL
    },
    // Make sure nulls in the context are fine
    { "d862c2e36b0a42f7827c67ebc8d44df7",
      "7a5b95e4e8378123",
      "4142434445464700",
      16,
      "12d418c6d0738a2e4d85b2d0170f76e1"
    },
    // ... and give a different result than without
    { "d862c2e36b0a42f7827c67ebc8d44df7",
      "7a5b95e4e8378123",
      "41424344454647",
      16,
      "abfa1c479a6e3ffb98a11dee7d196408"
    },
    // Try weird lengths
    { "d0ec8a34f6cc9a8c96",
      "49711798cc6251",
      "933d4a2f30d22f089cfba842791116adc121e0",
      23,
      "c9a46ed0757bd1812f1f21b4d41e62125fec8364a21db7"
    },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_vector); i++) {
    // Decode the test vector.
    string subkey_secret;
    string label;
    string context;
    ASSERT_TRUE(DecodeHexString(test_vector[i].subkey_secret, &subkey_secret));
    ASSERT_TRUE(DecodeHexString(test_vector[i].label, &label));
    ASSERT_TRUE(DecodeHexString(test_vector[i].context, &context));
    size_t result_len = test_vector[i].result_len;
    bool expect_ok = test_vector[i].expected != NULL;
    string expected;
    if (expect_ok) {
      ASSERT_TRUE(DecodeHexString(test_vector[i].expected, &expected));
    }

    string result;
    bool ok = CryptoUtils::ExportKeyingMaterial(subkey_secret,
                                                label,
                                                context,
                                                result_len,
                                                &result);
    EXPECT_EQ(expect_ok, ok);
    if (expect_ok) {
      EXPECT_EQ(result_len, result.length());
      test::CompareCharArraysWithHexError("HKDF output",
                                          result.data(),
                                          result.length(),
                                          expected.data(),
                                          expected.length());
    }
  }
}

}  // namespace
}  // namespace test
}  // namespace net
