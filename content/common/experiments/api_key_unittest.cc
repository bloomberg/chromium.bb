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

const char* kSampleAPIKey =
    "Signature|https://valid.example.com|Frobulate|1458766277";

const char* kExpectedAPIKeySignature = "Signature";
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

}  // namespace content
