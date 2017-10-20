// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_util.h"

#include <memory>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kTestAndroidRealm[] = "android://hash@com.example.beta.android";
const char kTestFederationURL[] = "https://google.com/";
const char kTestUsername[] = "Username";
const char kTestUsername2[] = "Username2";
const char kTestPassword[] = "12345";

autofill::PasswordForm GetTestAndroidCredentials(const char* signon_realm) {
  autofill::PasswordForm form;
  form.scheme = autofill::PasswordForm::SCHEME_HTML;
  form.signon_realm = signon_realm;
  form.username_value = base::ASCIIToUTF16(kTestUsername);
  form.password_value = base::ASCIIToUTF16(kTestPassword);
  return form;
}

}  // namespace

using password_manager::UnorderedPasswordFormElementsAre;

TEST(PasswordManagerUtil, TrimUsernameOnlyCredentials) {
  std::vector<std::unique_ptr<autofill::PasswordForm>> forms;
  std::vector<std::unique_ptr<autofill::PasswordForm>> expected_forms;
  forms.push_back(std::make_unique<autofill::PasswordForm>(
      GetTestAndroidCredentials(kTestAndroidRealm)));
  expected_forms.push_back(std::make_unique<autofill::PasswordForm>(
      GetTestAndroidCredentials(kTestAndroidRealm)));

  autofill::PasswordForm username_only;
  username_only.scheme = autofill::PasswordForm::SCHEME_USERNAME_ONLY;
  username_only.signon_realm = kTestAndroidRealm;
  username_only.username_value = base::ASCIIToUTF16(kTestUsername2);
  forms.push_back(std::make_unique<autofill::PasswordForm>(username_only));

  username_only.federation_origin =
      url::Origin::Create(GURL(kTestFederationURL));
  username_only.skip_zero_click = false;
  forms.push_back(std::make_unique<autofill::PasswordForm>(username_only));
  username_only.skip_zero_click = true;
  expected_forms.push_back(
      std::make_unique<autofill::PasswordForm>(username_only));

  password_manager_util::TrimUsernameOnlyCredentials(&forms);

  EXPECT_THAT(forms, UnorderedPasswordFormElementsAre(&expected_forms));
}

TEST(PasswordManagerUtil, CalculateSyncPasswordHash) {
  const char* kPlainText[] = {"", "password", "password", "secret"};
  const char* kSalt[] = {"", "salt", "123", "456"};

  constexpr uint64_t kExpectedHash[] = {
      UINT64_C(0x1c610a7950), UINT64_C(0x1927dc525e), UINT64_C(0xf72f81aa6),
      UINT64_C(0x3645af77f),
  };

  static_assert(arraysize(kPlainText) == arraysize(kSalt),
                "Arrays must have the same size");
  static_assert(arraysize(kPlainText) == arraysize(kExpectedHash),
                "Arrays must have the same size");

  for (size_t i = 0; i < arraysize(kPlainText); ++i) {
    SCOPED_TRACE(i);
    base::string16 text = base::UTF8ToUTF16(kPlainText[i]);
    EXPECT_EQ(kExpectedHash[i],
              password_manager_util::CalculateSyncPasswordHash(text, kSalt[i]));
  }
}
