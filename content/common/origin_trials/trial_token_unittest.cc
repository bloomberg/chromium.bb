// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/origin_trials/trial_token.h"

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
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

// This is a good trial token, signed with the above test private key.
const char* kSampleToken =
    "1|UsEO0cNxoUtBnHDJdGPWTlXuLENjXcEIPL7Bs7sbvicPCcvAtyqhQuTJ9h/u1R3VZpWigtI+"
    "SdUwk7Dyk/qbDw==|https://valid.example.com|Frobulate|1458766277";
const uint8_t kExpectedVersion = 1;
const char* kExpectedSignature =
    "UsEO0cNxoUtBnHDJdGPWTlXuLENjXcEIPL7Bs7sbvicPCcvAtyqhQuTJ9h/u1R3VZpWigtI+S"
    "dUwk7Dyk/qbDw==";
const char* kExpectedData = "https://valid.example.com|Frobulate|1458766277";
const char* kExpectedFeatureName = "Frobulate";
const char* kExpectedOrigin = "https://valid.example.com";
const uint64_t kExpectedExpiry = 1458766277;

// The token should not be valid for this origin, or for this feature.
const char* kInvalidOrigin = "https://invalid.example.com";
const char* kInsecureOrigin = "http://valid.example.com";
const char* kInvalidFeatureName = "Grokalyze";

// The token should be valid if the current time is kValidTimestamp or earlier.
double kValidTimestamp = 1458766276.0;

// The token should be invalid if the current time is kInvalidTimestamp or
// later.
double kInvalidTimestamp = 1458766278.0;

// Well-formed trial token with an invalid signature.
const char* kInvalidSignatureToken =
    "1|CO8hDne98QeFeOJ0DbRZCBN3uE0nyaPgaLlkYhSWnbRoDfEAg+TXELaYfQPfEvKYFauBg/"
    "hnxmba765hz0mXMc==|https://valid.example.com|Frobulate|1458766277";

// Various ill-formed trial tokens. These should all fail to parse.
const char* kInvalidTokens[] = {
    // Invalid - only one part
    "abcde",
    // Not enough parts
    "https://valid.example.com|FeatureName|1458766277",
    "Signature|https://valid.example.com|FeatureName|1458766277",
    // Non-numeric version
    "a|Signature|https://valid.example.com|FeatureName|1458766277",
    "1x|Signature|https://valid.example.com|FeatureName|1458766277",
    // Unsupported version (< min, > max, negative, overflow)
    "0|Signature|https://valid.example.com|FeatureName|1458766277",
    "2|Signature|https://valid.example.com|FeatureName|1458766277",
    "-1|Signature|https://valid.example.com|FeatureName|1458766277",
    "99999|Signature|https://valid.example.com|FeatureName|1458766277",
    // Delimiter in feature name
    "1|Signature|https://valid.example.com|Feature|Name|1458766277",
    // Extra string field
    "1|Signature|https://valid.example.com|FeatureName|1458766277|ExtraField",
    // Extra numeric field
    "1|Signature|https://valid.example.com|FeatureName|1458766277|1458766277",
    // Non-numeric expiry timestamp
    "1|Signature|https://valid.example.com|FeatureName|abcdefghij",
    "1|Signature|https://valid.example.com|FeatureName|1458766277x",
    // Negative expiry timestamp
    "1|Signature|https://valid.example.com|FeatureName|-1458766277",
    // Origin not a proper origin URL
    "1|Signature|abcdef|FeatureName|1458766277",
    "1|Signature|data:text/plain,abcdef|FeatureName|1458766277",
    "1|Signature|javascript:alert(1)|FeatureName|1458766277"};
const size_t kNumInvalidTokens = arraysize(kInvalidTokens);

}  // namespace

class TrialTokenTest : public testing::Test {
 public:
  TrialTokenTest()
      : public_key_(
            base::StringPiece(reinterpret_cast<const char*>(kTestPublicKey),
                              arraysize(kTestPublicKey))) {}

 protected:
  bool ValidateOrigin(TrialToken* token, const char* origin) {
    return token->ValidateOrigin(origin);
  }

  bool ValidateFeatureName(TrialToken* token, const char* feature_name) {
    return token->ValidateFeatureName(feature_name);
  }

  bool ValidateDate(TrialToken* token, const base::Time& now) {
    return token->ValidateDate(now);
  }

  bool ValidateSignature(TrialToken* token,
                         const base::StringPiece& public_key) {
    return token->ValidateSignature(public_key);
  }

  const base::StringPiece& public_key() { return public_key_; };

 private:
  base::StringPiece public_key_;
};

TEST_F(TrialTokenTest, ParseEmptyString) {
  scoped_ptr<TrialToken> empty_token = TrialToken::Parse("");
  EXPECT_FALSE(empty_token);
}

TEST_F(TrialTokenTest, ParseInvalidStrings) {
  for (size_t i = 0; i < kNumInvalidTokens; ++i) {
    scoped_ptr<TrialToken> empty_token = TrialToken::Parse(kInvalidTokens[i]);
    EXPECT_FALSE(empty_token) << "Invalid trial token should not parse: "
                              << kInvalidTokens[i];
  }
}

TEST_F(TrialTokenTest, ParseValidToken) {
  scoped_ptr<TrialToken> token = TrialToken::Parse(kSampleToken);
  ASSERT_TRUE(token);
  EXPECT_EQ(kExpectedVersion, token->version());
  EXPECT_EQ(kExpectedFeatureName, token->feature_name());
  EXPECT_EQ(kExpectedSignature, token->signature());
  EXPECT_EQ(kExpectedData, token->data());
  EXPECT_EQ(GURL(kExpectedOrigin), token->origin());
  EXPECT_EQ(kExpectedExpiry, token->expiry_timestamp());
}

TEST_F(TrialTokenTest, ValidateValidToken) {
  scoped_ptr<TrialToken> token = TrialToken::Parse(kSampleToken);
  ASSERT_TRUE(token);
  EXPECT_TRUE(ValidateOrigin(token.get(), kExpectedOrigin));
  EXPECT_FALSE(ValidateOrigin(token.get(), kInvalidOrigin));
  EXPECT_FALSE(ValidateOrigin(token.get(), kInsecureOrigin));
  EXPECT_TRUE(ValidateFeatureName(token.get(), kExpectedFeatureName));
  EXPECT_FALSE(ValidateFeatureName(token.get(), kInvalidFeatureName));
  EXPECT_FALSE(ValidateFeatureName(
      token.get(), base::ToUpperASCII(kExpectedFeatureName).c_str()));
  EXPECT_FALSE(ValidateFeatureName(
      token.get(), base::ToLowerASCII(kExpectedFeatureName).c_str()));
  EXPECT_TRUE(
      ValidateDate(token.get(), base::Time::FromDoubleT(kValidTimestamp)));
  EXPECT_FALSE(
      ValidateDate(token.get(), base::Time::FromDoubleT(kInvalidTimestamp)));
}

TEST_F(TrialTokenTest, TokenIsAppropriateForOriginAndFeature) {
  scoped_ptr<TrialToken> token = TrialToken::Parse(kSampleToken);
  ASSERT_TRUE(token);
  EXPECT_TRUE(token->IsAppropriate(kExpectedOrigin, kExpectedFeatureName));
  EXPECT_FALSE(token->IsAppropriate(kExpectedOrigin,
                                    base::ToUpperASCII(kExpectedFeatureName)));
  EXPECT_FALSE(token->IsAppropriate(kExpectedOrigin,
                                    base::ToLowerASCII(kExpectedFeatureName)));
  EXPECT_FALSE(token->IsAppropriate(kInvalidOrigin, kExpectedFeatureName));
  EXPECT_FALSE(token->IsAppropriate(kInsecureOrigin, kExpectedFeatureName));
  EXPECT_FALSE(token->IsAppropriate(kExpectedOrigin, kInvalidFeatureName));
}

TEST_F(TrialTokenTest, ValidateValidSignature) {
  scoped_ptr<TrialToken> token = TrialToken::Parse(kSampleToken);
  ASSERT_TRUE(token);
  EXPECT_TRUE(ValidateSignature(token.get(), public_key()));
}

TEST_F(TrialTokenTest, ValidateInvalidSignature) {
  scoped_ptr<TrialToken> token = TrialToken::Parse(kInvalidSignatureToken);
  ASSERT_TRUE(token);
  EXPECT_FALSE(ValidateSignature(token.get(), public_key()));
}

TEST_F(TrialTokenTest, ValidateSignatureOnWrongKey) {
  scoped_ptr<TrialToken> token = TrialToken::Parse(kSampleToken);
  ASSERT_TRUE(token);
  // Signature will be invalid if tested against the real public key
  EXPECT_FALSE(token->IsValid(base::Time::FromDoubleT(kValidTimestamp)));
}

TEST_F(TrialTokenTest, ValidateWhenNotExpired) {
  scoped_ptr<TrialToken> token = TrialToken::Parse(kSampleToken);
  ASSERT_TRUE(token);
}

}  // namespace content
