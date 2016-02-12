// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/origin_trials/trial_token_validator.h"

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "content/public/renderer/content_renderer_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

namespace {

// This is a sample public key for testing the API. The corresponding private
// key (use this to generate new samples for this test file) is:
//
//  0x83, 0x67, 0xf4, 0xcd, 0x2a, 0x1f, 0x0e, 0x04, 0x0d, 0x43, 0x13,
//  0x4c, 0x67, 0xc4, 0xf4, 0x28, 0xc9, 0x90, 0x15, 0x02, 0xe2, 0xba,
//  0xfd, 0xbb, 0xfa, 0xbc, 0x92, 0x76, 0x8a, 0x2c, 0x4b, 0xc7, 0x75,
//  0x10, 0xac, 0xf9, 0x3a, 0x1c, 0xb8, 0xa9, 0x28, 0x70, 0xd2, 0x9a,
//  0xd0, 0x0b, 0x59, 0xe1, 0xac, 0x2b, 0xb7, 0xd5, 0xca, 0x1f, 0x64,
//  0x90, 0x08, 0x8e, 0xa8, 0xe0, 0x56, 0x3a, 0x04, 0xd0
const uint8_t kTestPublicKey[] = {
    0x75, 0x10, 0xac, 0xf9, 0x3a, 0x1c, 0xb8, 0xa9, 0x28, 0x70, 0xd2,
    0x9a, 0xd0, 0x0b, 0x59, 0xe1, 0xac, 0x2b, 0xb7, 0xd5, 0xca, 0x1f,
    0x64, 0x90, 0x08, 0x8e, 0xa8, 0xe0, 0x56, 0x3a, 0x04, 0xd0,
};

//  0x21, 0xee, 0xfa, 0x81, 0x6a, 0xff, 0xdf, 0xb8, 0xc1, 0xdd, 0x75,
//  0x05, 0x04, 0x29, 0x68, 0x67, 0x60, 0x85, 0x91, 0xd0, 0x50, 0x16,
//  0x0a, 0xcf, 0xa2, 0x37, 0xa3, 0x2e, 0x11, 0x7a, 0x17, 0x96, 0x50,
//  0x07, 0x4d, 0x76, 0x55, 0x56, 0x42, 0x17, 0x2d, 0x8a, 0x9c, 0x47,
//  0x96, 0x25, 0xda, 0x70, 0xaa, 0xb9, 0xfd, 0x53, 0x5d, 0x51, 0x3e,
//  0x16, 0xab, 0xb4, 0x86, 0xea, 0xf3, 0x35, 0xc6, 0xca
const uint8_t kTestPublicKey2[] = {
    0x50, 0x07, 0x4d, 0x76, 0x55, 0x56, 0x42, 0x17, 0x2d, 0x8a, 0x9c,
    0x47, 0x96, 0x25, 0xda, 0x70, 0xaa, 0xb9, 0xfd, 0x53, 0x5d, 0x51,
    0x3e, 0x16, 0xab, 0xb4, 0x86, 0xea, 0xf3, 0x35, 0xc6, 0xca,
};

// This is a good trial token, signed with the above test private key.
// TODO(iclelland): This token expires in 2033. Update it or find a way
// to autogenerate it before then.
const char kSampleToken[] =
    "1|w694328Rl8l2vd96nkbAumpwvOOnvhWTj9/pfBRkvcWMDAsmiMEhZGEPzdBRy5Yao6il5qC"
    "OyS6Ah7uuHf7JAQ==|https://valid.example.com|Frobulate|2000000000";

// The token should be valid for this origin and for this feature.
const char kAppropriateOrigin[] = "https://valid.example.com";
const char kAppropriateFeatureName[] = "Frobulate";

const char kInappropriateFeatureName[] = "Grokalyze";
const char kInappropriateOrigin[] = "https://invalid.example.com";
const char kInsecureOrigin[] = "http://valid.example.com";

// Well-formed trial token with an invalid signature.
const char kInvalidSignatureToken[] =
    "1|CO8hDne98QeFeOJ0DbRZCBN3uE0nyaPgaLlkYhSWnbRoDfEAg+TXELaYfQPfEvKYFauBg/h"
    "nxmba765hz0mXMc==|https://valid.example.com|Frobulate|2000000000";

// Well-formed, but expired, trial token. (Expired in 2001)
const char kExpiredToken[] =
    "1|Vtzq/H0qMxsMXPThIgGEvI13d3Fd8K3W11/0E+FrJJXqBpx6n/dFkeFkEUsPaP3KeT8PCPF"
    "1zpZ7kVgWYRLpAA==|https://valid.example.com|Frobulate|1000000000";

const char kUnparsableToken[] = "abcde";

class TestContentRendererClient : public content::ContentRendererClient {
 public:
  base::StringPiece GetOriginTrialPublicKey() override {
    return base::StringPiece(reinterpret_cast<const char*>(key_),
                             arraysize(kTestPublicKey));
  }
  void SetOriginTrialPublicKey(const uint8_t* key) { key_ = key; }
  const uint8_t* key_ = nullptr;
};

}  // namespace

class TrialTokenValidatorTest : public testing::Test {
 public:
  TrialTokenValidatorTest() {
    SetPublicKey(kTestPublicKey);
    SetRendererClientForTesting(&test_content_renderer_client_);
  }

  void SetPublicKey(const uint8_t* key) {
    test_content_renderer_client_.SetOriginTrialPublicKey(key);
  }

  TrialTokenValidator trial_token_validator_;

 private:
  TestContentRendererClient test_content_renderer_client_;
};

TEST_F(TrialTokenValidatorTest, ValidateValidToken) {
  EXPECT_TRUE(trial_token_validator_.validateToken(
      kSampleToken, kAppropriateOrigin, kAppropriateFeatureName));
}

TEST_F(TrialTokenValidatorTest, ValidateInappropriateOrigin) {
  EXPECT_FALSE(TrialTokenValidator().validateToken(
      kSampleToken, kInappropriateOrigin, kAppropriateFeatureName));
  EXPECT_FALSE(TrialTokenValidator().validateToken(
      kSampleToken, kInsecureOrigin, kAppropriateFeatureName));
}

TEST_F(TrialTokenValidatorTest, ValidateInappropriateFeature) {
  EXPECT_FALSE(TrialTokenValidator().validateToken(
      kSampleToken, kAppropriateOrigin, kInappropriateFeatureName));
}

TEST_F(TrialTokenValidatorTest, ValidateInvalidSignature) {
  EXPECT_FALSE(TrialTokenValidator().validateToken(
      kInvalidSignatureToken, kAppropriateOrigin, kAppropriateFeatureName));
}

TEST_F(TrialTokenValidatorTest, ValidateUnparsableToken) {
  EXPECT_FALSE(TrialTokenValidator().validateToken(
      kUnparsableToken, kAppropriateOrigin, kAppropriateFeatureName));
}

TEST_F(TrialTokenValidatorTest, ValidateExpiredToken) {
  EXPECT_FALSE(TrialTokenValidator().validateToken(
      kExpiredToken, kAppropriateOrigin, kAppropriateFeatureName));
}

TEST_F(TrialTokenValidatorTest, ValidateValidTokenWithIncorrectKey) {
  SetPublicKey(kTestPublicKey2);
  EXPECT_FALSE(TrialTokenValidator().validateToken(
      kSampleToken, kAppropriateOrigin, kAppropriateFeatureName));
}

}  // namespace content
