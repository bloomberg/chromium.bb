// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/net/gaia/google_service_auth_error.h"

#include <string>

#include "base/scoped_ptr.h"
#include "base/values.h"
#include "chrome/test/values_test_util.h"
#include "net/base/net_errors.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using test::ExpectStringValue;

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
  ExpectStringValue("NONE", *value, "state");
}

TEST_F(GoogleServiceAuthErrorTest, ConnectionFailed) {
  GoogleServiceAuthError error(
      GoogleServiceAuthError::FromConnectionError(net::OK));
  scoped_ptr<DictionaryValue> value(error.ToValue());
  EXPECT_EQ(2u, value->size());
  ExpectStringValue("CONNECTION_FAILED", *value, "state");
  ExpectStringValue("net::OK", *value, "networkError");
}

TEST_F(GoogleServiceAuthErrorTest, CaptchaChallenge) {
  GoogleServiceAuthError error(
      GoogleServiceAuthError::FromCaptchaChallenge(
          "captcha_token", GURL("http://www.google.com"),
          GURL("http://www.bing.com")));
  scoped_ptr<DictionaryValue> value(error.ToValue());
  EXPECT_EQ(2u, value->size());
  ExpectStringValue("CAPTCHA_REQUIRED", *value, "state");
  DictionaryValue* captcha_value = NULL;
  EXPECT_TRUE(value->GetDictionary("captcha", &captcha_value));
  ASSERT_TRUE(captcha_value);
  ExpectStringValue("captcha_token", *captcha_value, "token");
  ExpectStringValue("http://www.google.com/", *captcha_value, "imageUrl");
  ExpectStringValue("http://www.bing.com/", *captcha_value, "unlockUrl");
}

}  // namespace
