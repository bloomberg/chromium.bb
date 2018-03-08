// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/password_manager/core/browser/password_manager_util.h"

#include <memory>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/scoped_task_environment.h"
#include "components/password_manager/core/browser/mock_password_store.h"
#include "components/password_manager/core/browser/password_manager_test_utils.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace password_manager_util {
namespace {

constexpr char kTestAndroidRealm[] = "android://hash@com.example.beta.android";
constexpr char kTestFederationURL[] = "https://google.com/";
constexpr char kTestURL[] = "https://example.com/";
constexpr char kTestUsername[] = "Username";
constexpr char kTestUsername2[] = "Username2";
constexpr char kTestPassword[] = "12345";

autofill::PasswordForm GetTestAndroidCredentials(const char* signon_realm) {
  autofill::PasswordForm form;
  form.scheme = autofill::PasswordForm::SCHEME_HTML;
  form.signon_realm = signon_realm;
  form.username_value = base::ASCIIToUTF16(kTestUsername);
  form.password_value = base::ASCIIToUTF16(kTestPassword);
  return form;
}

// The argument is std::vector<autofill::PasswordForm*>*. The caller is
// responsible for the lifetime of all the password forms.
ACTION_P(AppendForm, form) {
  arg0->push_back(std::make_unique<autofill::PasswordForm>(form));
}

}  // namespace

using password_manager::UnorderedPasswordFormElementsAre;
using testing::_;
using testing::DoAll;
using testing::Return;

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

  TrimUsernameOnlyCredentials(&forms);

  EXPECT_THAT(forms, UnorderedPasswordFormElementsAre(&expected_forms));
}

TEST(PasswordManagerUtil, CleanBlacklistedUsernamePassword) {
  autofill::PasswordForm blacklisted;
  blacklisted.blacklisted_by_user = true;
  blacklisted.signon_realm = kTestURL;
  blacklisted.origin = GURL(kTestURL);

  autofill::PasswordForm blacklisted_with_username = blacklisted;
  blacklisted_with_username.username_value = base::ASCIIToUTF16(kTestUsername);

  autofill::PasswordForm blacklisted_with_password = blacklisted;
  blacklisted_with_password.password_value = base::ASCIIToUTF16(kTestPassword);

  base::test::ScopedTaskEnvironment scoped_task_environment;
  TestingPrefServiceSimple prefs;
  prefs.registry()->RegisterBooleanPref(
      password_manager::prefs::kBlacklistedCredentialsStripped, false);
  auto password_store = base::MakeRefCounted<
      testing::StrictMock<password_manager::MockPasswordStore>>();
  ASSERT_TRUE(
      password_store->Init(syncer::SyncableService::StartSyncFlare(), nullptr));

  EXPECT_CALL(*password_store, FillBlacklistLogins(_))
      .WillOnce(DoAll(AppendForm(blacklisted),
                      AppendForm(blacklisted_with_username),
                      AppendForm(blacklisted_with_password), Return(true)));
  // Wrong credentials are to be cleaned.
  EXPECT_CALL(*password_store, RemoveLogin(blacklisted_with_username));
  EXPECT_CALL(*password_store, RemoveLogin(blacklisted_with_password));
  EXPECT_CALL(*password_store, AddLogin(blacklisted)).Times(2);
  CleanUserDataInBlacklistedCredentials(password_store.get(), &prefs, 0);
  scoped_task_environment.RunUntilIdle();

  EXPECT_FALSE(prefs.GetBoolean(
      password_manager::prefs::kBlacklistedCredentialsStripped));

  // Clean up with no credentials to be updated.
  EXPECT_CALL(*password_store, FillBlacklistLogins(_))
      .WillOnce(DoAll(AppendForm(blacklisted), Return(true)));
  CleanUserDataInBlacklistedCredentials(password_store.get(), &prefs, 0);
  scoped_task_environment.RunUntilIdle();

  EXPECT_TRUE(prefs.GetBoolean(
      password_manager::prefs::kBlacklistedCredentialsStripped));

  // Clean up again. Nothing should happen.
  EXPECT_CALL(*password_store, FillBlacklistLogins(_)).Times(0);
  CleanUserDataInBlacklistedCredentials(password_store.get(), &prefs, 0);
  scoped_task_environment.RunUntilIdle();

  password_store->ShutdownOnUIThread();
}

}  // namespace password_manager_util
