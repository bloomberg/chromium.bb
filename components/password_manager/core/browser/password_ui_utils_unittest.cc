// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/password_ui_utils.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager {

TEST(GetShownOriginTest, RemovePrefixes) {
  const struct {
    const std::string input;
    const std::string output;
  } kTestCases[] = {
      {"http://subdomain.example.com:80/login/index.html?h=90&ind=hello#first",
       "subdomain.example.com"},
      {"https://www.example.com", "example.com"},
      {"https://mobile.example.com", "example.com"},
      {"https://m.example.com", "example.com"},
      {"https://m.subdomain.example.net", "subdomain.example.net"},
      {"https://mobile.de", "mobile.de"},
      {"https://www.de", "www.de"},
      {"https://m.de", "m.de"},
      {"https://www.mobile.de", "mobile.de"},
      {"https://m.mobile.de", "mobile.de"},
      {"https://m.www.de", "www.de"},
      {"https://Mobile.example.de", "example.de"},
      {"https://WWW.Example.DE", "example.de"}};

  for (const auto& test_case : kTestCases) {
    autofill::PasswordForm password_form;
    password_form.signon_realm = "https://non.android.signon.com";
    password_form.origin = GURL(test_case.input);
    bool is_android_uri;
    EXPECT_EQ(test_case.output,
              GetShownOrigin(password_form, "", &is_android_uri))
        << "for input " << test_case.input;
    EXPECT_FALSE(is_android_uri) << "for input " << test_case.input;
    EXPECT_EQ(test_case.output, GetShownOrigin(password_form.origin, ""));
  }
}

TEST(GetShownOriginTest, OriginFromAndroidForm) {
  autofill::PasswordForm android_form;
  android_form.signon_realm =
      "android://"
      "m3HSJL1i83hdltRq0-o9czGb-8KJDKra4t_"
      "3JRlnPKcjI8PZm6XBHXx6zG4UuMXaDEZjR1wuXDre9G9zvN7AQw=="
      "@com.example.android";
  bool is_android_uri;
  EXPECT_EQ(GetShownOrigin(android_form, "", &is_android_uri),
            "android://com.example.android");
  EXPECT_TRUE(is_android_uri);
}

}  // namespace password_manager
