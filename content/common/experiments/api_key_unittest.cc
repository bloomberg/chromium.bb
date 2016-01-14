// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/experiments/api_key.h"

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

// This is a good key, signed with the above test private key.
const char* kSampleAPIKey =
    "UsEO0cNxoUtBnHDJdGPWTlXuLENjXcEIPL7Bs7sbvicPCcvAtyqhQuTJ9h/u1R3VZpWigtI+S"
    "dUwk7Dyk/qbDw==|https://valid.example.com|Frobulate|1458766277";
const char* kExpectedAPIKeySignature =
    "UsEO0cNxoUtBnHDJdGPWTlXuLENjXcEIPL7Bs7sbvicPCcvAtyqhQuTJ9h/u1R3VZpWigtI+S"
    "dUwk7Dyk/qbDw==";
const char* kExpectedAPIKeyData =
    "https://valid.example.com|Frobulate|1458766277";
const char* kExpectedAPIName = "Frobulate";
const char* kExpectedOrigin = "https://valid.example.com";
const uint64_t kExpectedExpiry = 1458766277;

// The key should not be valid for this origin, or for this API.
const char* kInvalidOrigin = "https://invalid.example.com";
const char* kInsecureOrigin = "http://valid.example.com";
const char* kInvalidAPIName = "Grokalyze";

// The key should be valid if the current time is kValidTimestamp or earlier.
double kValidTimestamp = 1458766276.0;

// The key should be invalid if the current time is kInvalidTimestamp or later.
double kInvalidTimestamp = 1458766278.0;

// Well-formed API key with an invalid signature.
const char* kInvalidSignatureAPIKey =
    "CO8hDne98QeFeOJ0DbRZCBN3uE0nyaPgaLlkYhSWnbRoDfEAg+TXELaYfQPfEvKYFauBg/hnx"
    "mba765hz0mXMc==|https://valid.example.com|Frobulate|1458766277";

// Various ill-formed API keys. These should all fail to parse.
const char* kInvalidAPIKeys[] = {
    // Invalid - only one part
    "abcde",
    // Not enough parts
    "https://valid.example.com|APIName|1458766277",
    // Delimiter in API Name
    "Signature|https://valid.example.com|API|Name|1458766277",
    // Extra string field
    "Signature|https://valid.example.com|APIName|1458766277|SomethingElse",
    // Extra numeric field
    "Signature|https://valid.example.com|APIName|1458766277|1458766277",
    // Non-numeric expiry timestamp
    "Signature|https://valid.example.com|APIName|abcdefghij",
    "Signature|https://valid.example.com|APIName|1458766277x",
    // Negative expiry timestamp
    "Signature|https://valid.example.com|APIName|-1458766277",
    // Origin not a proper origin URL
    "Signature|abcdef|APIName|1458766277",
    "Signature|data:text/plain,abcdef|APIName|1458766277",
    "Signature|javascript:alert(1)|APIName|1458766277"};
const size_t kNumInvalidAPIKeys = arraysize(kInvalidAPIKeys);

}  // namespace

class ApiKeyTest : public testing::Test {
 public:
  ApiKeyTest()
      : public_key_(
            base::StringPiece(reinterpret_cast<const char*>(kTestPublicKey),
                              arraysize(kTestPublicKey))) {}

 protected:
  bool ValidateOrigin(ApiKey* api_key, const char* origin) {
    return api_key->ValidateOrigin(origin);
  }

  bool ValidateApiName(ApiKey* api_key, const char* api_name) {
    return api_key->ValidateApiName(api_name);
  }

  bool ValidateDate(ApiKey* api_key, const base::Time& now) {
    return api_key->ValidateDate(now);
  }

  bool ValidateSignature(ApiKey* api_key, const base::StringPiece& public_key) {
    return api_key->ValidateSignature(public_key);
  }

  const base::StringPiece& public_key() { return public_key_; };

 private:
  base::StringPiece public_key_;
};

TEST_F(ApiKeyTest, ParseEmptyString) {
  scoped_ptr<ApiKey> empty_key = ApiKey::Parse("");
  EXPECT_FALSE(empty_key);
}

TEST_F(ApiKeyTest, ParseInvalidStrings) {
  for (size_t i = 0; i < kNumInvalidAPIKeys; ++i) {
    scoped_ptr<ApiKey> empty_key = ApiKey::Parse(kInvalidAPIKeys[i]);
    EXPECT_FALSE(empty_key) << "Invalid API Key should not parse: "
                            << kInvalidAPIKeys[i];
  }
}

TEST_F(ApiKeyTest, ParseValidKeyString) {
  scoped_ptr<ApiKey> key = ApiKey::Parse(kSampleAPIKey);
  ASSERT_TRUE(key);
  EXPECT_EQ(kExpectedAPIName, key->api_name());
  EXPECT_EQ(kExpectedAPIKeySignature, key->signature());
  EXPECT_EQ(kExpectedAPIKeyData, key->data());
  EXPECT_EQ(GURL(kExpectedOrigin), key->origin());
  EXPECT_EQ(kExpectedExpiry, key->expiry_timestamp());
}

TEST_F(ApiKeyTest, ValidateValidKey) {
  scoped_ptr<ApiKey> key = ApiKey::Parse(kSampleAPIKey);
  ASSERT_TRUE(key);
  EXPECT_TRUE(ValidateOrigin(key.get(), kExpectedOrigin));
  EXPECT_FALSE(ValidateOrigin(key.get(), kInvalidOrigin));
  EXPECT_FALSE(ValidateOrigin(key.get(), kInsecureOrigin));
  EXPECT_TRUE(ValidateApiName(key.get(), kExpectedAPIName));
  EXPECT_FALSE(ValidateApiName(key.get(), kInvalidAPIName));
  EXPECT_TRUE(
      ValidateDate(key.get(), base::Time::FromDoubleT(kValidTimestamp)));
  EXPECT_FALSE(
      ValidateDate(key.get(), base::Time::FromDoubleT(kInvalidTimestamp)));
}

TEST_F(ApiKeyTest, KeyIsAppropriateForOriginAndAPI) {
  scoped_ptr<ApiKey> key = ApiKey::Parse(kSampleAPIKey);
  ASSERT_TRUE(key);
  EXPECT_TRUE(key->IsAppropriate(kExpectedOrigin, kExpectedAPIName));
  EXPECT_TRUE(key->IsAppropriate(kExpectedOrigin,
                                 base::ToUpperASCII(kExpectedAPIName)));
  EXPECT_TRUE(key->IsAppropriate(kExpectedOrigin,
                                 base::ToLowerASCII(kExpectedAPIName)));
  EXPECT_FALSE(key->IsAppropriate(kInvalidOrigin, kExpectedAPIName));
  EXPECT_FALSE(key->IsAppropriate(kInsecureOrigin, kExpectedAPIName));
  EXPECT_FALSE(key->IsAppropriate(kExpectedOrigin, kInvalidAPIName));
}

TEST_F(ApiKeyTest, ValidateValidSignature) {
  scoped_ptr<ApiKey> key = ApiKey::Parse(kSampleAPIKey);
  ASSERT_TRUE(key);
  EXPECT_TRUE(ValidateSignature(key.get(), public_key()));
}

TEST_F(ApiKeyTest, ValidateInvalidSignature) {
  scoped_ptr<ApiKey> key = ApiKey::Parse(kInvalidSignatureAPIKey);
  ASSERT_TRUE(key);
  EXPECT_FALSE(ValidateSignature(key.get(), public_key()));
}

TEST_F(ApiKeyTest, ValidateSignatureOnWrongKey) {
  scoped_ptr<ApiKey> key = ApiKey::Parse(kSampleAPIKey);
  ASSERT_TRUE(key);
  // Signature will be invalid if tested against the real public key
  EXPECT_FALSE(key->IsValid(base::Time::FromDoubleT(kValidTimestamp)));
}

TEST_F(ApiKeyTest, ValidateWhenNotExpired) {
  scoped_ptr<ApiKey> key = ApiKey::Parse(kSampleAPIKey);
  ASSERT_TRUE(key);
}

}  // namespace content
