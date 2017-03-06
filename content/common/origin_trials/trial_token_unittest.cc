// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/origin_trials/trial_token.h"

#include <memory>

#include "base/macros.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/test/simple_test_clock.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/WebKit/public/platform/WebOriginTrialTokenStatus.h"
#include "url/gurl.h"

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
//
//  This private key can also be found in tools/origin_trials/eftest.key in
//  binary form. Please update that if changing the key.
//
//  To use this with a real browser, use --origin-trial-public-key with the
//  public key, base-64-encoded:
//  --origin-trial-public-key=dRCs+TocuKkocNKa0AtZ4awrt9XKH2SQCI6o4FY6BNA=
const uint8_t kTestPublicKey[] = {
    0x75, 0x10, 0xac, 0xf9, 0x3a, 0x1c, 0xb8, 0xa9, 0x28, 0x70, 0xd2,
    0x9a, 0xd0, 0x0b, 0x59, 0xe1, 0xac, 0x2b, 0xb7, 0xd5, 0xca, 0x1f,
    0x64, 0x90, 0x08, 0x8e, 0xa8, 0xe0, 0x56, 0x3a, 0x04, 0xd0,
};

// This is a valid, but incorrect, public key for testing signatures against.
// The corresponding private key is:
//
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
// Generate this token with the command (in tools/origin_trials):
// generate_token.py valid.example.com Frobulate --expire-timestamp=1458766277
const char* kSampleToken =
    "Ap+Q/Qm0ELadZql+dlEGSwnAVsFZKgCEtUZg8idQC3uekkIeSZIY1tftoYdrwhqj"
    "7FO5L22sNvkZZnacLvmfNwsAAABZeyJvcmlnaW4iOiAiaHR0cHM6Ly92YWxpZC5l"
    "eGFtcGxlLmNvbTo0NDMiLCAiZmVhdHVyZSI6ICJGcm9idWxhdGUiLCAiZXhwaXJ5"
    "IjogMTQ1ODc2NjI3N30=";
const uint8_t kSampleTokenSignature[] = {
    0x9f, 0x90, 0xfd, 0x09, 0xb4, 0x10, 0xb6, 0x9d, 0x66, 0xa9, 0x7e,
    0x76, 0x51, 0x06, 0x4b, 0x09, 0xc0, 0x56, 0xc1, 0x59, 0x2a, 0x00,
    0x84, 0xb5, 0x46, 0x60, 0xf2, 0x27, 0x50, 0x0b, 0x7b, 0x9e, 0x92,
    0x42, 0x1e, 0x49, 0x92, 0x18, 0xd6, 0xd7, 0xed, 0xa1, 0x87, 0x6b,
    0xc2, 0x1a, 0xa3, 0xec, 0x53, 0xb9, 0x2f, 0x6d, 0xac, 0x36, 0xf9,
    0x19, 0x66, 0x76, 0x9c, 0x2e, 0xf9, 0x9f, 0x37, 0x0b};

// This is a good subdomain trial token, signed with the above test private key.
// Generate this token with the command (in tools/origin_trials):
// generate_token.py example.com Frobulate --is-subdomain
//   --expire-timestamp=1458766277
const char* kSampleSubdomainToken =
    "Auu+j9nXAQoy5+t00MiWakZwFExcdNC8ENkRdK1gL4OMFHS0AbZCscslDTcP1fjN"
    "FjpbmQG+VCPk1NrldVXZng4AAABoeyJvcmlnaW4iOiAiaHR0cHM6Ly9leGFtcGxl"
    "LmNvbTo0NDMiLCAiaXNTdWJkb21haW4iOiB0cnVlLCAiZmVhdHVyZSI6ICJGcm9i"
    "dWxhdGUiLCAiZXhwaXJ5IjogMTQ1ODc2NjI3N30=";
const uint8_t kSampleSubdomainTokenSignature[] = {
    0xeb, 0xbe, 0x8f, 0xd9, 0xd7, 0x01, 0x0a, 0x32, 0xe7, 0xeb, 0x74,
    0xd0, 0xc8, 0x96, 0x6a, 0x46, 0x70, 0x14, 0x4c, 0x5c, 0x74, 0xd0,
    0xbc, 0x10, 0xd9, 0x11, 0x74, 0xad, 0x60, 0x2f, 0x83, 0x8c, 0x14,
    0x74, 0xb4, 0x01, 0xb6, 0x42, 0xb1, 0xcb, 0x25, 0x0d, 0x37, 0x0f,
    0xd5, 0xf8, 0xcd, 0x16, 0x3a, 0x5b, 0x99, 0x01, 0xbe, 0x54, 0x23,
    0xe4, 0xd4, 0xda, 0xe5, 0x75, 0x55, 0xd9, 0x9e, 0x0e};

// This is a good trial token, explicitly not a subdomain, signed with the above
// test private key. Generate this token with the command:
// generate_token.py valid.example.com Frobulate --no-subdomain
//   --expire-timestamp=1458766277
const char* kSampleNonSubdomainToken =
    "AreD979D7tO0luSZTr1+/+J6E0SSj/GEUyLK41o1hXFzXw1R7Z1hCDHs0gXWVSu1"
    "lvH52Winvy39tHbsU2gJJQYAAABveyJvcmlnaW4iOiAiaHR0cHM6Ly92YWxpZC5l"
    "eGFtcGxlLmNvbTo0NDMiLCAiaXNTdWJkb21haW4iOiBmYWxzZSwgImZlYXR1cmUi"
    "OiAiRnJvYnVsYXRlIiwgImV4cGlyeSI6IDE0NTg3NjYyNzd9";
const uint8_t kSampleNonSubdomainTokenSignature[] = {
    0xb7, 0x83, 0xf7, 0xbf, 0x43, 0xee, 0xd3, 0xb4, 0x96, 0xe4, 0x99,
    0x4e, 0xbd, 0x7e, 0xff, 0xe2, 0x7a, 0x13, 0x44, 0x92, 0x8f, 0xf1,
    0x84, 0x53, 0x22, 0xca, 0xe3, 0x5a, 0x35, 0x85, 0x71, 0x73, 0x5f,
    0x0d, 0x51, 0xed, 0x9d, 0x61, 0x08, 0x31, 0xec, 0xd2, 0x05, 0xd6,
    0x55, 0x2b, 0xb5, 0x96, 0xf1, 0xf9, 0xd9, 0x68, 0xa7, 0xbf, 0x2d,
    0xfd, 0xb4, 0x76, 0xec, 0x53, 0x68, 0x09, 0x25, 0x06};

const char* kExpectedFeatureName = "Frobulate";
const char* kExpectedOrigin = "https://valid.example.com";
const char* kExpectedSubdomainOrigin = "https://example.com";
const char* kExpectedMultipleSubdomainOrigin =
    "https://part1.part2.part3.example.com";
const uint64_t kExpectedExpiry = 1458766277;

// The token should not be valid for this origin, or for this feature.
const char* kInvalidOrigin = "https://invalid.example.com";
const char* kInsecureOrigin = "http://valid.example.com";
const char* kIncorrectPortOrigin = "https://valid.example.com:444";
const char* kIncorrectDomainOrigin = "https://valid.example2.com";
const char* kInvalidTLDOrigin = "https://com";
const char* kInvalidFeatureName = "Grokalyze";

// The token should be valid if the current time is kValidTimestamp or earlier.
double kValidTimestamp = 1458766276.0;

// The token should be invalid if the current time is kInvalidTimestamp or
// later.
double kInvalidTimestamp = 1458766278.0;

// Well-formed trial token with an invalid signature.
const char* kInvalidSignatureToken =
    "Ap+Q/Qm0ELadZql+dlEGSwnAVsFZKgCEtUZg8idQC3uekkIeSZIY1tftoYdrwhqj"
    "7FO5L22sNvkZZnacLvmfNwsAAABaeyJvcmlnaW4iOiAiaHR0cHM6Ly92YWxpZC5l"
    "eGFtcGxlLmNvbTo0NDMiLCAiZmVhdHVyZSI6ICJGcm9idWxhdGV4IiwgImV4cGly"
    "eSI6IDE0NTg3NjYyNzd9";

// Trial token truncated in the middle of the length field; too short to
// possibly be valid.
const char kTruncatedToken[] =
    "Ap+Q/Qm0ELadZql+dlEGSwnAVsFZKgCEtUZg8idQC3uekkIeSZIY1tftoYdrwhqj"
    "7FO5L22sNvkZZnacLvmfNwsA";

// Trial token with an incorrectly-declared length, but with a valid signature.
const char kIncorrectLengthToken[] =
    "Ao06eNl/CZuM88qurWKX4RfoVEpHcVHWxdOTrEXZkaC1GUHyb/8L4sthADiVWdc9"
    "kXFyF1BW5bbraqp6MBVr3wEAAABaeyJvcmlnaW4iOiAiaHR0cHM6Ly92YWxpZC5l"
    "eGFtcGxlLmNvbTo0NDMiLCAiZmVhdHVyZSI6ICJGcm9idWxhdGUiLCAiZXhwaXJ5"
    "IjogMTQ1ODc2NjI3N30=";

// Trial token with a misidentified version (42).
const char kIncorrectVersionToken[] =
    "KlH8wVLT5o59uDvlJESorMDjzgWnvG1hmIn/GiT9Ng3f45ratVeiXCNTeaJheOaG"
    "A6kX4ir4Amv8aHVC+OJHZQkAAABZeyJvcmlnaW4iOiAiaHR0cHM6Ly92YWxpZC5l"
    "eGFtcGxlLmNvbTo0NDMiLCAiZmVhdHVyZSI6ICJGcm9idWxhdGUiLCAiZXhwaXJ5"
    "IjogMTQ1ODc2NjI3N30=";

const char kSampleTokenJSON[] =
    "{\"origin\": \"https://valid.example.com:443\", \"feature\": "
    "\"Frobulate\", \"expiry\": 1458766277}";

const char kSampleNonSubdomainTokenJSON[] =
    "{\"origin\": \"https://valid.example.com:443\", \"isSubdomain\": false, "
    "\"feature\": \"Frobulate\", \"expiry\": 1458766277}";

const char kSampleSubdomainTokenJSON[] =
    "{\"origin\": \"https://example.com:443\", \"isSubdomain\": true, "
    "\"feature\": \"Frobulate\", \"expiry\": 1458766277}";

// Various ill-formed trial tokens. These should all fail to parse.
const char* kInvalidTokens[] = {
    // Invalid - Not JSON at all
    "abcde",
    // Invalid JSON
    "{",
    // Not an object
    "\"abcde\"",
    "123.4",
    "[0, 1, 2]",
    // Missing keys
    "{}",
    "{\"something\": 1}",
    "{\"origin\": \"https://a.a\"}",
    "{\"origin\": \"https://a.a\", \"feature\": \"a\"}",
    "{\"origin\": \"https://a.a\", \"expiry\": 1458766277}",
    "{\"feature\": \"FeatureName\", \"expiry\": 1458766277}",
    // Incorrect types
    "{\"origin\": 1, \"feature\": \"a\", \"expiry\": 1458766277}",
    "{\"origin\": \"https://a.a\", \"feature\": 1, \"expiry\": 1458766277}",
    "{\"origin\": \"https://a.a\", \"feature\": \"a\", \"expiry\": \"1\"}",
    "{\"origin\": \"https://a.a\", \"isSubdomain\": \"true\", \"feature\": "
    "\"a\", \"expiry\": 1458766277}",
    "{\"origin\": \"https://a.a\", \"isSubdomain\": 1, \"feature\": \"a\", "
    "\"expiry\": 1458766277}",
    // Negative expiry timestamp
    "{\"origin\": \"https://a.a\", \"feature\": \"a\", \"expiry\": -1}",
    // Origin not a proper origin URL
    "{\"origin\": \"abcdef\", \"feature\": \"a\", \"expiry\": 1458766277}",
    "{\"origin\": \"data:text/plain,abcdef\", \"feature\": \"a\", \"expiry\": "
    "1458766277}",
    "{\"origin\": \"javascript:alert(1)\", \"feature\": \"a\", \"expiry\": "
    "1458766277}",
};

}  // namespace

class TrialTokenTest : public testing::TestWithParam<const char*> {
 public:
  TrialTokenTest()
      : expected_origin_(GURL(kExpectedOrigin)),
        expected_subdomain_origin_(GURL(kExpectedSubdomainOrigin)),
        expected_multiple_subdomain_origin_(
            GURL(kExpectedMultipleSubdomainOrigin)),
        invalid_origin_(GURL(kInvalidOrigin)),
        insecure_origin_(GURL(kInsecureOrigin)),
        incorrect_port_origin_(GURL(kIncorrectPortOrigin)),
        incorrect_domain_origin_(GURL(kIncorrectDomainOrigin)),
        invalid_tld_origin_(GURL(kInvalidTLDOrigin)),
        expected_expiry_(base::Time::FromDoubleT(kExpectedExpiry)),
        valid_timestamp_(base::Time::FromDoubleT(kValidTimestamp)),
        invalid_timestamp_(base::Time::FromDoubleT(kInvalidTimestamp)),
        expected_signature_(
            std::string(reinterpret_cast<const char*>(kSampleTokenSignature),
                        arraysize(kSampleTokenSignature))),
        expected_subdomain_signature_(std::string(
            reinterpret_cast<const char*>(kSampleSubdomainTokenSignature),
            arraysize(kSampleSubdomainTokenSignature))),
        expected_nonsubdomain_signature_(std::string(
            reinterpret_cast<const char*>(kSampleNonSubdomainTokenSignature),
            arraysize(kSampleNonSubdomainTokenSignature))),
        correct_public_key_(
            base::StringPiece(reinterpret_cast<const char*>(kTestPublicKey),
                              arraysize(kTestPublicKey))),
        incorrect_public_key_(
            base::StringPiece(reinterpret_cast<const char*>(kTestPublicKey2),
                              arraysize(kTestPublicKey2))) {}

 protected:
  blink::WebOriginTrialTokenStatus Extract(const std::string& token_text,
                                           base::StringPiece public_key,
                                           std::string* token_payload,
                                           std::string* token_signature) {
    return TrialToken::Extract(token_text, public_key, token_payload,
                               token_signature);
  }

  blink::WebOriginTrialTokenStatus ExtractIgnorePayload(
      const std::string& token_text,
      base::StringPiece public_key) {
    std::string token_payload;
    std::string token_signature;
    return Extract(token_text, public_key, &token_payload, &token_signature);
  }

  std::unique_ptr<TrialToken> Parse(const std::string& token_payload) {
    return TrialToken::Parse(token_payload);
  }

  bool ValidateOrigin(TrialToken* token, const url::Origin origin) {
    return token->ValidateOrigin(origin);
  }

  bool ValidateFeatureName(TrialToken* token, const char* feature_name) {
    return token->ValidateFeatureName(feature_name);
  }

  bool ValidateDate(TrialToken* token, const base::Time& now) {
    return token->ValidateDate(now);
  }

  base::StringPiece correct_public_key() { return correct_public_key_; }
  base::StringPiece incorrect_public_key() { return incorrect_public_key_; }

  const url::Origin expected_origin_;
  const url::Origin expected_subdomain_origin_;
  const url::Origin expected_multiple_subdomain_origin_;
  const url::Origin invalid_origin_;
  const url::Origin insecure_origin_;
  const url::Origin incorrect_port_origin_;
  const url::Origin incorrect_domain_origin_;
  const url::Origin invalid_tld_origin_;

  const base::Time expected_expiry_;
  const base::Time valid_timestamp_;
  const base::Time invalid_timestamp_;

  std::string expected_signature_;
  std::string expected_subdomain_signature_;
  std::string expected_nonsubdomain_signature_;

 private:
  base::StringPiece correct_public_key_;
  base::StringPiece incorrect_public_key_;
};

// Test the extraction of the signed payload from token strings. This includes
// checking the included version identifier, payload length, and cryptographic
// signature.

// Test verification of signature and extraction of token JSON from signed
// token.
TEST_F(TrialTokenTest, ValidateValidSignature) {
  std::string token_payload;
  std::string token_signature;
  blink::WebOriginTrialTokenStatus status = Extract(
      kSampleToken, correct_public_key(), &token_payload, &token_signature);
  ASSERT_EQ(blink::WebOriginTrialTokenStatus::Success, status);
  EXPECT_STREQ(kSampleTokenJSON, token_payload.c_str());
  EXPECT_EQ(expected_signature_, token_signature);
}

TEST_F(TrialTokenTest, ValidateSubdomainValidSignature) {
  std::string token_payload;
  std::string token_signature;
  blink::WebOriginTrialTokenStatus status =
      Extract(kSampleSubdomainToken, correct_public_key(), &token_payload,
              &token_signature);
  ASSERT_EQ(blink::WebOriginTrialTokenStatus::Success, status);
  EXPECT_STREQ(kSampleSubdomainTokenJSON, token_payload.c_str());
  EXPECT_EQ(expected_subdomain_signature_, token_signature);
}

TEST_F(TrialTokenTest, ValidateNonSubdomainValidSignature) {
  std::string token_payload;
  std::string token_signature;
  blink::WebOriginTrialTokenStatus status =
      Extract(kSampleNonSubdomainToken, correct_public_key(), &token_payload,
              &token_signature);
  ASSERT_EQ(blink::WebOriginTrialTokenStatus::Success, status);
  EXPECT_STREQ(kSampleNonSubdomainTokenJSON, token_payload.c_str());
  EXPECT_EQ(expected_nonsubdomain_signature_, token_signature);
}

TEST_F(TrialTokenTest, ValidateInvalidSignature) {
  blink::WebOriginTrialTokenStatus status =
      ExtractIgnorePayload(kInvalidSignatureToken, correct_public_key());
  EXPECT_EQ(blink::WebOriginTrialTokenStatus::InvalidSignature, status);
}

TEST_F(TrialTokenTest, ValidateSignatureWithIncorrectKey) {
  blink::WebOriginTrialTokenStatus status =
      ExtractIgnorePayload(kSampleToken, incorrect_public_key());
  EXPECT_EQ(blink::WebOriginTrialTokenStatus::InvalidSignature, status);
}

TEST_F(TrialTokenTest, ValidateEmptyToken) {
  blink::WebOriginTrialTokenStatus status =
      ExtractIgnorePayload("", correct_public_key());
  EXPECT_EQ(blink::WebOriginTrialTokenStatus::Malformed, status);
}

TEST_F(TrialTokenTest, ValidateShortToken) {
  blink::WebOriginTrialTokenStatus status =
      ExtractIgnorePayload(kTruncatedToken, correct_public_key());
  EXPECT_EQ(blink::WebOriginTrialTokenStatus::Malformed, status);
}

TEST_F(TrialTokenTest, ValidateUnsupportedVersion) {
  blink::WebOriginTrialTokenStatus status =
      ExtractIgnorePayload(kIncorrectVersionToken, correct_public_key());
  EXPECT_EQ(blink::WebOriginTrialTokenStatus::WrongVersion, status);
}

TEST_F(TrialTokenTest, ValidateSignatureWithIncorrectLength) {
  blink::WebOriginTrialTokenStatus status =
      ExtractIgnorePayload(kIncorrectLengthToken, correct_public_key());
  EXPECT_EQ(blink::WebOriginTrialTokenStatus::Malformed, status);
}

// Test parsing of fields from JSON token.

TEST_F(TrialTokenTest, ParseEmptyString) {
  std::unique_ptr<TrialToken> empty_token = Parse("");
  EXPECT_FALSE(empty_token);
}

TEST_P(TrialTokenTest, ParseInvalidString) {
  std::unique_ptr<TrialToken> empty_token = Parse(GetParam());
  EXPECT_FALSE(empty_token) << "Invalid trial token should not parse.";
}

INSTANTIATE_TEST_CASE_P(, TrialTokenTest, ::testing::ValuesIn(kInvalidTokens));

TEST_F(TrialTokenTest, ParseValidToken) {
  std::unique_ptr<TrialToken> token = Parse(kSampleTokenJSON);
  ASSERT_TRUE(token);
  EXPECT_EQ(kExpectedFeatureName, token->feature_name());
  EXPECT_FALSE(token->match_subdomains());
  EXPECT_EQ(expected_origin_, token->origin());
  EXPECT_EQ(expected_expiry_, token->expiry_time());
}

TEST_F(TrialTokenTest, ParseValidNonSubdomainToken) {
  std::unique_ptr<TrialToken> token = Parse(kSampleNonSubdomainTokenJSON);
  ASSERT_TRUE(token);
  EXPECT_EQ(kExpectedFeatureName, token->feature_name());
  EXPECT_FALSE(token->match_subdomains());
  EXPECT_EQ(expected_origin_, token->origin());
  EXPECT_EQ(expected_expiry_, token->expiry_time());
}

TEST_F(TrialTokenTest, ParseValidSubdomainToken) {
  std::unique_ptr<TrialToken> token = Parse(kSampleSubdomainTokenJSON);
  ASSERT_TRUE(token);
  EXPECT_EQ(kExpectedFeatureName, token->feature_name());
  EXPECT_TRUE(token->match_subdomains());
  EXPECT_EQ(kExpectedSubdomainOrigin, token->origin().Serialize());
  EXPECT_EQ(expected_subdomain_origin_, token->origin());
  EXPECT_EQ(expected_expiry_, token->expiry_time());
}

TEST_F(TrialTokenTest, ValidateValidToken) {
  std::unique_ptr<TrialToken> token = Parse(kSampleTokenJSON);
  ASSERT_TRUE(token);
  EXPECT_TRUE(ValidateOrigin(token.get(), expected_origin_));
  EXPECT_FALSE(ValidateOrigin(token.get(), invalid_origin_));
  EXPECT_FALSE(ValidateOrigin(token.get(), insecure_origin_));
  EXPECT_FALSE(ValidateOrigin(token.get(), incorrect_port_origin_));
  EXPECT_FALSE(ValidateOrigin(token.get(), incorrect_domain_origin_));
  EXPECT_FALSE(ValidateOrigin(token.get(), invalid_tld_origin_));
  EXPECT_TRUE(ValidateFeatureName(token.get(), kExpectedFeatureName));
  EXPECT_FALSE(ValidateFeatureName(token.get(), kInvalidFeatureName));
  EXPECT_FALSE(ValidateFeatureName(
      token.get(), base::ToUpperASCII(kExpectedFeatureName).c_str()));
  EXPECT_FALSE(ValidateFeatureName(
      token.get(), base::ToLowerASCII(kExpectedFeatureName).c_str()));
  EXPECT_TRUE(ValidateDate(token.get(), valid_timestamp_));
  EXPECT_FALSE(ValidateDate(token.get(), invalid_timestamp_));
}

TEST_F(TrialTokenTest, ValidateValidSubdomainToken) {
  std::unique_ptr<TrialToken> token = Parse(kSampleSubdomainTokenJSON);
  ASSERT_TRUE(token);
  EXPECT_TRUE(ValidateOrigin(token.get(), expected_origin_));
  EXPECT_TRUE(ValidateOrigin(token.get(), expected_subdomain_origin_));
  EXPECT_TRUE(ValidateOrigin(token.get(), expected_multiple_subdomain_origin_));
  EXPECT_FALSE(ValidateOrigin(token.get(), insecure_origin_));
  EXPECT_FALSE(ValidateOrigin(token.get(), incorrect_port_origin_));
  EXPECT_FALSE(ValidateOrigin(token.get(), incorrect_domain_origin_));
  EXPECT_FALSE(ValidateOrigin(token.get(), invalid_tld_origin_));
}

TEST_F(TrialTokenTest, TokenIsValid) {
  std::unique_ptr<TrialToken> token = Parse(kSampleTokenJSON);
  ASSERT_TRUE(token);
  EXPECT_EQ(blink::WebOriginTrialTokenStatus::Success,
            token->IsValid(expected_origin_, valid_timestamp_));
  EXPECT_EQ(blink::WebOriginTrialTokenStatus::WrongOrigin,
            token->IsValid(invalid_origin_, valid_timestamp_));
  EXPECT_EQ(blink::WebOriginTrialTokenStatus::WrongOrigin,
            token->IsValid(insecure_origin_, valid_timestamp_));
  EXPECT_EQ(blink::WebOriginTrialTokenStatus::WrongOrigin,
            token->IsValid(incorrect_port_origin_, valid_timestamp_));
  EXPECT_EQ(blink::WebOriginTrialTokenStatus::Expired,
            token->IsValid(expected_origin_, invalid_timestamp_));
}

TEST_F(TrialTokenTest, SubdomainTokenIsValid) {
  std::unique_ptr<TrialToken> token = Parse(kSampleSubdomainTokenJSON);
  ASSERT_TRUE(token);
  EXPECT_EQ(blink::WebOriginTrialTokenStatus::Success,
            token->IsValid(expected_origin_, valid_timestamp_));
  EXPECT_EQ(blink::WebOriginTrialTokenStatus::Success,
            token->IsValid(expected_subdomain_origin_, valid_timestamp_));
  EXPECT_EQ(
      blink::WebOriginTrialTokenStatus::Success,
      token->IsValid(expected_multiple_subdomain_origin_, valid_timestamp_));
  EXPECT_EQ(blink::WebOriginTrialTokenStatus::WrongOrigin,
            token->IsValid(incorrect_domain_origin_, valid_timestamp_));
  EXPECT_EQ(blink::WebOriginTrialTokenStatus::WrongOrigin,
            token->IsValid(insecure_origin_, valid_timestamp_));
  EXPECT_EQ(blink::WebOriginTrialTokenStatus::WrongOrigin,
            token->IsValid(incorrect_port_origin_, valid_timestamp_));
  EXPECT_EQ(blink::WebOriginTrialTokenStatus::Expired,
            token->IsValid(expected_origin_, invalid_timestamp_));
}

// Test overall extraction, to ensure output status matches returned token, and
// signature is provided.
TEST_F(TrialTokenTest, ExtractValidToken) {
  blink::WebOriginTrialTokenStatus status;
  std::unique_ptr<TrialToken> token =
      TrialToken::From(kSampleToken, correct_public_key(), &status);
  EXPECT_TRUE(token);
  EXPECT_EQ(blink::WebOriginTrialTokenStatus::Success, status);
  EXPECT_EQ(expected_signature_, token->signature());
}

TEST_F(TrialTokenTest, ExtractInvalidSignature) {
  blink::WebOriginTrialTokenStatus status;
  std::unique_ptr<TrialToken> token =
      TrialToken::From(kSampleToken, incorrect_public_key(), &status);
  EXPECT_FALSE(token);
  EXPECT_EQ(blink::WebOriginTrialTokenStatus::InvalidSignature, status);
}

TEST_F(TrialTokenTest, ExtractMalformedToken) {
  blink::WebOriginTrialTokenStatus status;
  std::unique_ptr<TrialToken> token =
      TrialToken::From(kIncorrectLengthToken, correct_public_key(), &status);
  EXPECT_FALSE(token);
  EXPECT_EQ(blink::WebOriginTrialTokenStatus::Malformed, status);
}

}  // namespace content
