// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/gaia/oauth_request_signer.h"

#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(OAuthRequestSignerTest, SignGet1) {
  GURL request_url("https://www.google.com/accounts/o8/GetOAuthToken");
  OAuthRequestSigner::Parameters parameters;
  parameters["scope"] = "https://www.google.com/accounts/OAuthLogin";
  parameters["oauth_nonce"] = "2oiE_aHdk5qRTz0L9C8Lq0g";
  parameters["xaouth_display_name"] = "Chromium";
  parameters["oauth_timestamp"] = "1308152953";
  std::string signed_text;
  ASSERT_TRUE(OAuthRequestSigner::Sign(
                  request_url,
                  parameters,
                  OAuthRequestSigner::HMAC_SHA1_SIGNATURE,
                  OAuthRequestSigner::GET_METHOD,
                  "johndoe",  // oauth_consumer_key
                  "53cR3t",  // consumer secret
                  "4/VGY0MsQadcmO8VnCv9gnhoEooq1v",  // oauth_token
                  "c5e0531ff55dfbb4054e", // token secret
                  &signed_text));
  ASSERT_EQ("https://www.google.com/accounts/o8/GetOAuthToken"
            "?oauth_consumer_key=johndoe"
            "&oauth_nonce=2oiE_aHdk5qRTz0L9C8Lq0g"
            "&oauth_signature_method=HMAC-SHA1"
            "&oauth_timestamp=1308152953"
            "&oauth_token=4%2FVGY0MsQadcmO8VnCv9gnhoEooq1v"
            "&oauth_version=1.0"
            "&scope=https%3A%2F%2Fwww.google.com%2Faccounts%2FOAuthLogin"
            "&xaouth_display_name=Chromium"
            "&oauth_signature=y9GCmlGSvNuTAotxsBMyxb6j%2BE8%3D",
            signed_text);
}

TEST(OAuthRequestSignerTest, SignGet2) {
  GURL request_url("https://www.google.com/accounts/OAuthGetAccessToken");
  OAuthRequestSigner::Parameters parameters;
  parameters["oauth_timestamp"] = "1308147831";
  parameters["oauth_nonce"] = "4d4hZW9DygWQujP2tz06UN";
  std::string signed_text;
  ASSERT_TRUE(OAuthRequestSigner::Sign(
                  request_url,
                  parameters,
                  OAuthRequestSigner::HMAC_SHA1_SIGNATURE,
                  OAuthRequestSigner::GET_METHOD,
                  "anonymous",  // oauth_consumer_key
                  "anonymous",  // consumer secret
                  "4/CcC-hgdj1TNnWaX8NTQ76YDXCBEK",  // oauth_token
                  "", // token secret
                  &signed_text));
  ASSERT_EQ(signed_text,
            "https://www.google.com/accounts/OAuthGetAccessToken"
            "?oauth_consumer_key=anonymous"
            "&oauth_nonce=4d4hZW9DygWQujP2tz06UN"
            "&oauth_signature_method=HMAC-SHA1"
            "&oauth_timestamp=1308147831"
            "&oauth_token=4%2FCcC-hgdj1TNnWaX8NTQ76YDXCBEK"
            "&oauth_version=1.0"
            "&oauth_signature=2KVN8YCOKgiNIA16EGTcfESvdvA%3D");
}

TEST(OAuthRequestSignerTest, ParseAndSignGet1) {
  GURL request_url("https://www.google.com/accounts/o8/GetOAuthToken"
                   "?scope=https://www.google.com/accounts/OAuthLogin"
                   "&oauth_nonce=2oiE_aHdk5qRTz0L9C8Lq0g"
                   "&xaouth_display_name=Chromium"
                   "&oauth_timestamp=1308152953");
  std::string signed_text;
  ASSERT_TRUE(OAuthRequestSigner::ParseAndSign(
                  request_url,
                  OAuthRequestSigner::HMAC_SHA1_SIGNATURE,
                  OAuthRequestSigner::GET_METHOD,
                  "anonymous",  // oauth_consumer_key
                  "anonymous",  // consumer secret
                  "4/CcC-hgdj1TNnWaX8NTQ76YDXCBEK",  // oauth_token
                  "", // token secret
                  &signed_text));
  ASSERT_EQ("https://www.google.com/accounts/o8/GetOAuthToken"
            "?oauth_consumer_key=anonymous"
            "&oauth_nonce=2oiE_aHdk5qRTz0L9C8Lq0g"
            "&oauth_signature_method=HMAC-SHA1"
            "&oauth_timestamp=1308152953"
            "&oauth_token=4%2FCcC-hgdj1TNnWaX8NTQ76YDXCBEK"
            "&oauth_version=1.0"
            "&scope=https%3A%2F%2Fwww.google.com%2Faccounts%2FOAuthLogin"
            "&xaouth_display_name=Chromium"
            "&oauth_signature=S%2B6dcftDfbINlavHuma4NLJ98Ys%3D",
            signed_text);
}

TEST(OAuthRequestSignerTest, ParseAndSignGet2) {
  GURL request_url("https://www.google.com/accounts/OAuthGetAccessToken"
                   "?oauth_timestamp=1308147831"
                   "&oauth_nonce=4d4hZW9DygWQujP2tz06UN");
  std::string signed_text;
  ASSERT_TRUE(OAuthRequestSigner::ParseAndSign(
                  request_url,
                  OAuthRequestSigner::HMAC_SHA1_SIGNATURE,
                  OAuthRequestSigner::GET_METHOD,
                  "anonymous",  // oauth_consumer_key
                  "anonymous",  // consumer secret
                  "4/CcC-hgdj1TNnWaX8NTQ76YDXCBEK",  // oauth_token
                  "", // token secret
                  &signed_text));
  ASSERT_EQ(signed_text,
            "https://www.google.com/accounts/OAuthGetAccessToken"
            "?oauth_consumer_key=anonymous"
            "&oauth_nonce=4d4hZW9DygWQujP2tz06UN"
            "&oauth_signature_method=HMAC-SHA1"
            "&oauth_timestamp=1308147831"
            "&oauth_token=4%2FCcC-hgdj1TNnWaX8NTQ76YDXCBEK"
            "&oauth_version=1.0"
            "&oauth_signature=2KVN8YCOKgiNIA16EGTcfESvdvA%3D");
}

TEST(OAuthRequestSignerTest, SignPost1) {
  GURL request_url("https://www.google.com/accounts/o8/GetOAuthToken");
  OAuthRequestSigner::Parameters parameters;
  parameters["scope"] = "https://www.google.com/accounts/OAuthLogin";
  parameters["oauth_nonce"] = "2oiE_aHdk5qRTz0L9C8Lq0g";
  parameters["xaouth_display_name"] = "Chromium";
  parameters["oauth_timestamp"] = "1308152953";
  std::string signed_text;
  ASSERT_TRUE(OAuthRequestSigner::Sign(
                  request_url,
                  parameters,
                  OAuthRequestSigner::HMAC_SHA1_SIGNATURE,
                  OAuthRequestSigner::POST_METHOD,
                  "anonymous",  // oauth_consumer_key
                  "anonymous",  // consumer secret
                  "4/X8x0r7bHif_VNCLjUMutxGkzo13d",  // oauth_token
                  "b7120598d47594bd3522", // token secret
                  &signed_text));
  ASSERT_EQ("oauth_consumer_key=anonymous"
            "&oauth_nonce=2oiE_aHdk5qRTz0L9C8Lq0g"
            "&oauth_signature_method=HMAC-SHA1"
            "&oauth_timestamp=1308152953"
            "&oauth_token=4%2FX8x0r7bHif_VNCLjUMutxGkzo13d"
            "&oauth_version=1.0"
            "&scope=https%3A%2F%2Fwww.google.com%2Faccounts%2FOAuthLogin"
            "&xaouth_display_name=Chromium"
            "&oauth_signature=F%2BINyO4xgon5wUxcdcxWC11Ep7Y%3D",
            signed_text);
}

TEST(OAuthRequestSignerTest, SignPost2) {
  GURL request_url("https://www.google.com/accounts/OAuthGetAccessToken");
  OAuthRequestSigner::Parameters parameters;
  parameters["oauth_timestamp"] = "1234567890";
  parameters["oauth_nonce"] = "17171717171717171";
  std::string signed_text;
  ASSERT_TRUE(OAuthRequestSigner::Sign(
                  request_url,
                  parameters,
                  OAuthRequestSigner::HMAC_SHA1_SIGNATURE,
                  OAuthRequestSigner::POST_METHOD,
                  "anonymous",  // oauth_consumer_key
                  "anonymous",  // consumer secret
                  "4/CcC-hgdj1TNnWaX8NTQ76YDXCBEK",  // oauth_token
                  "", // token secret
                  &signed_text));
  ASSERT_EQ(signed_text,
            "oauth_consumer_key=anonymous"
            "&oauth_nonce=17171717171717171"
            "&oauth_signature_method=HMAC-SHA1"
            "&oauth_timestamp=1234567890"
            "&oauth_token=4%2FCcC-hgdj1TNnWaX8NTQ76YDXCBEK"
            "&oauth_version=1.0"
            "&oauth_signature=BIuPHITrcptxSefd8H9Iazo8Pmo%3D");
}

TEST(OAuthRequestSignerTest, ParseAndSignPost1) {
  GURL request_url("https://www.google.com/accounts/o8/GetOAuthToken"
                   "?scope=https://www.google.com/accounts/OAuthLogin"
                   "&oauth_nonce=2oiE_aHdk5qRTz0L9C8Lq0g"
                   "&xaouth_display_name=Chromium"
                   "&oauth_timestamp=1308152953");
  std::string signed_text;
  ASSERT_TRUE(OAuthRequestSigner::ParseAndSign(
                  request_url,
                  OAuthRequestSigner::HMAC_SHA1_SIGNATURE,
                  OAuthRequestSigner::POST_METHOD,
                  "anonymous",  // oauth_consumer_key
                  "anonymous",  // consumer secret
                  "4/X8x0r7bHif_VNCLjUMutxGkzo13d",  // oauth_token
                  "b7120598d47594bd3522", // token secret
                  &signed_text));
  ASSERT_EQ("oauth_consumer_key=anonymous"
            "&oauth_nonce=2oiE_aHdk5qRTz0L9C8Lq0g"
            "&oauth_signature_method=HMAC-SHA1"
            "&oauth_timestamp=1308152953"
            "&oauth_token=4%2FX8x0r7bHif_VNCLjUMutxGkzo13d"
            "&oauth_version=1.0"
            "&scope=https%3A%2F%2Fwww.google.com%2Faccounts%2FOAuthLogin"
            "&xaouth_display_name=Chromium"
            "&oauth_signature=F%2BINyO4xgon5wUxcdcxWC11Ep7Y%3D",
            signed_text);
}

TEST(OAuthRequestSignerTest, ParseAndSignPost2) {
  GURL request_url("https://www.google.com/accounts/OAuthGetAccessToken"
                   "?oauth_timestamp=1234567890"
                   "&oauth_nonce=17171717171717171");
  std::string signed_text;
  ASSERT_TRUE(OAuthRequestSigner::ParseAndSign(
                  request_url,
                  OAuthRequestSigner::HMAC_SHA1_SIGNATURE,
                  OAuthRequestSigner::POST_METHOD,
                  "anonymous",  // oauth_consumer_key
                  "anonymous",  // consumer secret
                  "4/CcC-hgdj1TNnWaX8NTQ76YDXCBEK",  // oauth_token
                  "", // token secret
                  &signed_text));
  ASSERT_EQ(signed_text,
            "oauth_consumer_key=anonymous"
            "&oauth_nonce=17171717171717171"
            "&oauth_signature_method=HMAC-SHA1"
            "&oauth_timestamp=1234567890"
            "&oauth_token=4%2FCcC-hgdj1TNnWaX8NTQ76YDXCBEK"
            "&oauth_version=1.0"
            "&oauth_signature=BIuPHITrcptxSefd8H9Iazo8Pmo%3D");
}
