// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/passwords_private/passwords_private_utils.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/common/password_form.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace extensions {

TEST(CreateUrlCollectionFromFormTest, UrlsFromHtmlForm) {
  autofill::PasswordForm html_form;
  html_form.origin = GURL("http://example.com/LoginAuth");
  html_form.signon_realm = html_form.origin.GetOrigin().spec();

  api::passwords_private::UrlCollection html_urls =
      CreateUrlCollectionFromForm(html_form);
  EXPECT_EQ(html_urls.origin, "http://example.com/");
  EXPECT_EQ(html_urls.shown, "example.com");
  EXPECT_EQ(html_urls.link, "http://example.com/LoginAuth");
}

TEST(CreateUrlCollectionFromFormTest, UrlsFromFederatedForm) {
  autofill::PasswordForm federated_form;
  federated_form.signon_realm = "federation://example.com/google.com";
  federated_form.origin = GURL("https://example.com/");
  federated_form.federation_origin = url::Origin(GURL("https://google.com/"));

  api::passwords_private::UrlCollection federated_urls =
      CreateUrlCollectionFromForm(federated_form);
  EXPECT_EQ(federated_urls.origin, "federation://example.com/google.com");
  EXPECT_EQ(federated_urls.shown, "example.com");
  EXPECT_EQ(federated_urls.link, "https://example.com/");
}

TEST(CreateUrlCollectionFromFormTest, UrlsFromAndroidFormWithoutAffiliation) {
  autofill::PasswordForm android_form;
  android_form.signon_realm = "android://example_hash@com.example.android";

  api::passwords_private::UrlCollection android_urls =
      CreateUrlCollectionFromForm(android_form);
  EXPECT_EQ(android_urls.origin, "android://example_hash@com.example.android");
  EXPECT_THAT(android_urls.shown, testing::StartsWith("android.example.com"));
  EXPECT_THAT(android_urls.shown, testing::HasSubstr(l10n_util::GetStringUTF8(
                                      IDS_PASSWORDS_ANDROID_URI_SUFFIX)));
  EXPECT_EQ(
      android_urls.link,
      "https://play.google.com/store/apps/details?id=com.example.android");
}

TEST(CreateUrlCollectionFromFormTest, UrlsFromAndroidFormWithAffiliation) {
  autofill::PasswordForm android_form;
  android_form.affiliated_web_realm = "https://example.com";
  android_form.signon_realm = "android://example_hash@com.example.android";

  api::passwords_private::UrlCollection android_urls =
      CreateUrlCollectionFromForm(android_form);
  EXPECT_EQ(android_urls.origin, "android://example_hash@com.example.android");
  EXPECT_THAT(android_urls.shown, testing::StartsWith("example.com"));
  EXPECT_THAT(android_urls.shown, testing::HasSubstr(l10n_util::GetStringUTF8(
                                      IDS_PASSWORDS_ANDROID_URI_SUFFIX)));
  EXPECT_EQ(android_urls.link, "https://example.com/");
}

}  // namespace extensions
