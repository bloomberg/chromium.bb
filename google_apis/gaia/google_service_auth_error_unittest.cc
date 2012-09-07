// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "google_apis/gaia/google_service_auth_error.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/test/values_test_util.h"
#include "base/values.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using base::ExpectDictStringValue;

class GoogleServiceAuthErrorTest : public testing::Test {};

void TestSimpleState(GoogleServiceAuthError::State state) {
  GoogleServiceAuthError error(state);
  scoped_ptr<DictionaryValue> value(error.ToValue());
  EXPECT_EQ(1u, value->size());
  std::string state_str;
  EXPECT_TRUE(value->GetString("state", &state_str));
  EXPECT_FALSE(state_str.empty());
  EXPECT_NE("CONNECTION_FAILED", state_str);
  EXPECT_NE("CAPTCHA_REQUIRED", state_str);
}

TEST_F(GoogleServiceAuthErrorTest, SimpleToValue) {
  for (int i = GoogleServiceAuthError::NONE;
       i <= GoogleServiceAuthError::USER_NOT_SIGNED_UP; ++i) {
    TestSimpleState(static_cast<GoogleServiceAuthError::State>(i));
  }
}

TEST_F(GoogleServiceAuthErrorTest, None) {
  GoogleServiceAuthError error(GoogleServiceAuthError::None());
  scoped_ptr<DictionaryValue> value(error.ToValue());
  EXPECT_EQ(1u, value->size());
  ExpectDictStringValue("NONE", *value, "state");
}

TEST_F(GoogleServiceAuthErrorTest, ConnectionFailed) {
  GoogleServiceAuthError error(
      GoogleServiceAuthError::FromConnectionError(net::OK));
  scoped_ptr<DictionaryValue> value(error.ToValue());
  EXPECT_EQ(2u, value->size());
  ExpectDictStringValue("CONNECTION_FAILED", *value, "state");
  ExpectDictStringValue("net::OK", *value, "networkError");
}

TEST_F(GoogleServiceAuthErrorTest, CaptchaChallenge) {
  GoogleServiceAuthError error(
      GoogleServiceAuthError::FromClientLoginCaptchaChallenge(
          "captcha_token", GURL("http://www.google.com"),
          GURL("http://www.bing.com")));
  scoped_ptr<DictionaryValue> value(error.ToValue());
  EXPECT_EQ(2u, value->size());
  ExpectDictStringValue("CAPTCHA_REQUIRED", *value, "state");
  DictionaryValue* captcha_value = NULL;
  EXPECT_TRUE(value->GetDictionary("captcha", &captcha_value));
  ASSERT_TRUE(captcha_value);
  ExpectDictStringValue("captcha_token", *captcha_value, "token");
  ExpectDictStringValue("", *captcha_value, "audioUrl");
  ExpectDictStringValue("http://www.google.com/", *captcha_value, "imageUrl");
  ExpectDictStringValue("http://www.bing.com/", *captcha_value, "unlockUrl");
  ExpectDictIntegerValue(0, *captcha_value, "imageWidth");
  ExpectDictIntegerValue(0, *captcha_value, "imageHeight");
}

TEST_F(GoogleServiceAuthErrorTest, CaptchaChallenge2) {
  GoogleServiceAuthError error(
      GoogleServiceAuthError::FromCaptchaChallenge(
          "captcha_token", GURL("http://www.audio.com"),
          GURL("http://www.image.com"), 320, 200));
  scoped_ptr<DictionaryValue> value(error.ToValue());
  EXPECT_EQ(2u, value->size());
  ExpectDictStringValue("CAPTCHA_REQUIRED", *value, "state");
  DictionaryValue* captcha_value = NULL;
  EXPECT_TRUE(value->GetDictionary("captcha", &captcha_value));
  ASSERT_TRUE(captcha_value);
  ExpectDictStringValue("captcha_token", *captcha_value, "token");
  ExpectDictStringValue("http://www.audio.com/", *captcha_value, "audioUrl");
  ExpectDictStringValue("http://www.image.com/", *captcha_value, "imageUrl");
  ExpectDictIntegerValue(320, *captcha_value, "imageWidth");
  ExpectDictIntegerValue(200, *captcha_value, "imageHeight");
}

TEST_F(GoogleServiceAuthErrorTest, TwoFactorChallenge) {
  GoogleServiceAuthError error(
      GoogleServiceAuthError::FromSecondFactorChallenge(
          "two_factor_token", "prompt_text", "alternate_text", 10));
  scoped_ptr<DictionaryValue> value(error.ToValue());
  EXPECT_EQ(2u, value->size());
  ExpectDictStringValue("TWO_FACTOR", *value, "state");
  DictionaryValue* two_factor_value = NULL;
  EXPECT_TRUE(value->GetDictionary("two_factor", &two_factor_value));
  ASSERT_TRUE(two_factor_value);
  ExpectDictStringValue("two_factor_token", *two_factor_value, "token");
  ExpectDictStringValue("prompt_text", *two_factor_value, "promptText");
  ExpectDictStringValue("alternate_text", *two_factor_value, "alternateText");
  ExpectDictIntegerValue(10, *two_factor_value, "fieldLength");
}

TEST_F(GoogleServiceAuthErrorTest, ClientOAuthError) {
  // Test that a malformed/incomplete ClientOAuth response generates
  // a connection problem error.
  GoogleServiceAuthError error1(
      GoogleServiceAuthError::FromClientOAuthError("{}"));
  EXPECT_EQ(GoogleServiceAuthError::CONNECTION_FAILED, error1.state());

  // Test that a well formed ClientOAuth response generates an invalid
  // credentials error with the given error message.
  GoogleServiceAuthError error2(
      GoogleServiceAuthError::FromClientOAuthError(
          "{\"cause\":\"foo\",\"explanation\":\"error_message\"}"));
  EXPECT_EQ(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS, error2.state());
  EXPECT_EQ("error_message", error2.error_message());
}

}  // namespace
