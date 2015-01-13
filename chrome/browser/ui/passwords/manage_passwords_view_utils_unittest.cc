// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/passwords/manage_passwords_view_utils.h"

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/common/password_form.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/url_constants.h"

namespace {
const std::string kSchemeHostExample = "http://example.com";
}  // namespace

class GetHumanReadableOriginTest : public testing::Test {
 public:
  GetHumanReadableOriginTest() {
    form_template_.origin = GURL(kSchemeHostExample);
    form_template_.action = GURL(kSchemeHostExample);
    form_template_.username_element = base::ASCIIToUTF16("Email");
    form_template_.password_element = base::ASCIIToUTF16("Password");
    form_template_.submit_element = base::ASCIIToUTF16("signIn");
    form_template_.signon_realm = kSchemeHostExample;
  }
  const autofill::PasswordForm& form_remplate() { return form_template_; }

 private:
  autofill::PasswordForm form_template_;
};

TEST_F(GetHumanReadableOriginTest, OriginFromHtmlForm) {
  autofill::PasswordForm html_form(form_remplate());
  html_form.origin = GURL(kSchemeHostExample + "/LoginAuth");
  html_form.action = GURL(kSchemeHostExample + "/Login");
  html_form.scheme = autofill::PasswordForm::SCHEME_HTML;
  EXPECT_EQ(GetHumanReadableOrigin(html_form, ""), "example.com/LoginAuth");
}

TEST_F(GetHumanReadableOriginTest, OriginFromDigestForm) {
  autofill::PasswordForm non_html_form(form_remplate());
  non_html_form.scheme = autofill::PasswordForm::SCHEME_DIGEST;
  non_html_form.action = GURL();
  non_html_form.signon_realm = kSchemeHostExample + "42";
  EXPECT_EQ(GetHumanReadableOrigin(non_html_form, ""), "example.com");
}

TEST_F(GetHumanReadableOriginTest, OriginFromAndroidForm) {
  autofill::PasswordForm android_form(form_remplate());
  android_form.action = GURL();
  android_form.origin = GURL();
  android_form.scheme = autofill::PasswordForm::SCHEME_OTHER;
  android_form.signon_realm =
      "android://"
      "m3HSJL1i83hdltRq0-o9czGb-8KJDKra4t_"
      "3JRlnPKcjI8PZm6XBHXx6zG4UuMXaDEZjR1wuXDre9G9zvN7AQw=="
      "@com.example.android";
  EXPECT_EQ(GetHumanReadableOrigin(android_form, ""),
            "android://com.example.android");
}
