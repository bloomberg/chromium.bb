// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/loader/signed_exchange_signature_verifier.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {
namespace {

TEST(SignedExchangeSignatureVerifier, EncodeCanonicalExchangeHeaders) {
  SignedExchangeSignatureVerifier::Input input;
  input.method = "GET";
  input.url = "https://example.com/index.html";
  input.response_code = 200;
  input.response_headers.insert(
      std::make_pair("content-type", "text/html; charset=utf-8"));
  input.response_headers.insert(
      std::make_pair("content-encoding", "mi-sha256"));
  input.response_headers.insert(std::make_pair("unsigned-header", "foobar"));
  input.response_headers.insert(std::make_pair(
      "signed-headers", "\"content-type\", \"content-encoding\""));

  base::Optional<std::vector<uint8_t>> encoded =
      SignedExchangeSignatureVerifier::EncodeCanonicalExchangeHeaders(input);
  ASSERT_TRUE(encoded.has_value());

  static const uint8_t kExpected[] = {
      // clang-format off
      0x82, // array(2)
        0xa2, // map(2)
          0x44, 0x3a, 0x75, 0x72, 0x6c, // bytes ":url"
          0x58, 0x1e, 0x68, 0x74, 0x74, 0x70, 0x73, 0x3a, 0x2f, 0x2f, 0x65,
          0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x2e, 0x63, 0x6f, 0x6d, 0x2f,
          0x69, 0x6e, 0x64, 0x65, 0x78, 0x2e, 0x68, 0x74, 0x6d, 0x6c,
          // bytes "https://example.com/index.html"

          0x47, 0x3a, 0x6d, 0x65, 0x74, 0x68, 0x6f, 0x64, // bytes ":method"
          0x43, 0x47, 0x45, 0x54, // bytes "GET"

        0xa3, // map(3)
          0x47, 0x3a, 0x73,0x74, 0x61, 0x74, 0x75, 0x73, // bytes ":status"
          0x43, 0x32, 0x30, 0x30, // bytes "200"

          0x4c, 0x63, 0x6f, 0x6e, 0x74, 0x65, 0x6e, 0x74, 0x2d, 0x74, 0x79,
          0x70, 0x65, // bytes "content-type"
          0x58, 0x18, 0x74, 0x65, 0x78, 0x74, 0x2f, 0x68, 0x74, 0x6d, 0x6c,
          0x3b, 0x20, 0x63, 0x68, 0x61, 0x72, 0x73, 0x65, 0x74, 0x3d, 0x75,
          0x74, 0x66, 0x2d, 0x38, // bytes "text/html; charset=utf-8"

          0x50, 0x63, 0x6f, 0x6e, 0x74, 0x65, 0x6e, 0x74, 0x2d, 0x65, 0x6e,
          0x63, 0x6f, 0x64, 0x69, 0x6e, 0x67, // bytes "content-encoding"
          0x49, 0x6d, 0x69, 0x2d, 0x73, 0x68, 0x61, 0x32, 0x35, 0x36,
          // bytes "mi-sha256"
      // clang-format on
  };
  EXPECT_THAT(*encoded,
              testing::ElementsAreArray(kExpected, arraysize(kExpected)));
}

}  // namespace
}  // namespace content
