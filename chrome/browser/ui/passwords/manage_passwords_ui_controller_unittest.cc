// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/passwords/manage_passwords_icon.h"
#include "chrome/browser/ui/passwords/manage_passwords_icon_mock.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/content/common/credential_manager_types.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/browser/stub_password_manager_driver.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const int64 kSlowNavigationDelayInMS = 2000;
const int64 kQuickNavigationDelayInMS = 500;

}  // namespace

class ManagePasswordsUIControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  ManagePasswordsUIControllerTest() {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();

    // Create the test UIController here so that it's bound to
    // |test_web_contents_|, and will be retrieved correctly via
    // ManagePasswordsUIController::FromWebContents in |controller()|.
    new ManagePasswordsUIControllerMock(web_contents());

    test_local_form_.origin = GURL("http://example.com");
    test_local_form_.username_value = base::ASCIIToUTF16("username");
    test_local_form_.password_value = base::ASCIIToUTF16("12345");

    test_federated_form_.origin = GURL("http://example.com");
    test_federated_form_.username_value = base::ASCIIToUTF16("username");
    test_federated_form_.federation_url = GURL("https://federation.test/");

    // We need to be on a "webby" URL for most tests.
    content::WebContentsTester::For(web_contents())
        ->NavigateAndCommit(GURL("http://example.com"));
  }

  autofill::PasswordForm& test_local_form() { return test_local_form_; }
  autofill::PasswordForm& test_federated_form() { return test_federated_form_; }
  password_manager::CredentialInfo* credential_info() const {
    return credential_info_.get();
  }

  ManagePasswordsUIControllerMock* controller() {
    return static_cast<ManagePasswordsUIControllerMock*>(
        ManagePasswordsUIController::FromWebContents(web_contents()));
  }

  void CredentialCallback(const password_manager::CredentialInfo& info) {
    credential_info_.reset(new password_manager::CredentialInfo(info));
  }

 private:
  autofill::PasswordForm test_local_form_;
  autofill::PasswordForm test_federated_form_;
  scoped_ptr<password_manager::CredentialInfo> credential_info_;
};

TEST_F(ManagePasswordsUIControllerTest, DefaultState) {
  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, controller()->state());
  EXPECT_FALSE(controller()->PasswordPendingUserDecision());
  EXPECT_EQ(GURL::EmptyGURL(), controller()->origin());

  ManagePasswordsIconMock mock;
  controller()->UpdateIconAndBubbleState(&mock);
  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, mock.state());
}

TEST_F(ManagePasswordsUIControllerTest, PasswordAutofilled) {
  base::string16 kTestUsername = base::ASCIIToUTF16("test_username");
  autofill::PasswordFormMap map;
  map[kTestUsername] = &test_local_form();
  controller()->OnPasswordAutofilled(map);

  EXPECT_EQ(password_manager::ui::MANAGE_STATE, controller()->state());
  EXPECT_FALSE(controller()->PasswordPendingUserDecision());
  EXPECT_EQ(test_local_form().origin, controller()->origin());
  EXPECT_EQ(1u, controller()->best_matches().size());
  ASSERT_EQ(1u, controller()->best_matches().count(kTestUsername));

  // Controller should store a separate copy of the form as it doesn't own it.
  EXPECT_NE(&test_local_form(),
            controller()->best_matches().find(kTestUsername)->second);

  ManagePasswordsIconMock mock;
  controller()->UpdateIconAndBubbleState(&mock);
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, mock.state());
}

TEST_F(ManagePasswordsUIControllerTest, PasswordSubmitted) {
  password_manager::StubPasswordManagerClient client;
  password_manager::StubPasswordManagerDriver driver;
  scoped_ptr<password_manager::PasswordFormManager> test_form_manager(
      new password_manager::PasswordFormManager(
          NULL, &client, driver.AsWeakPtr(), test_local_form(), false));
  controller()->OnPasswordSubmitted(test_form_manager.Pass());
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_STATE,
            controller()->state());
  EXPECT_TRUE(controller()->PasswordPendingUserDecision());

  // TODO(mkwst): This should be the value of test_local_form().origin, but
  // it's being masked by the stub implementation of
  // ManagePasswordsUIControllerMock::PendingCredentials.
  EXPECT_EQ(GURL::EmptyGURL(), controller()->origin());

  ManagePasswordsIconMock mock;
  controller()->UpdateIconAndBubbleState(&mock);
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_STATE,
            mock.state());
}

TEST_F(ManagePasswordsUIControllerTest, PasswordSaved) {
  password_manager::StubPasswordManagerClient client;
  password_manager::StubPasswordManagerDriver driver;
  scoped_ptr<password_manager::PasswordFormManager> test_form_manager(
      new password_manager::PasswordFormManager(
          NULL, &client, driver.AsWeakPtr(), test_local_form(), false));
  controller()->OnPasswordSubmitted(test_form_manager.Pass());

  ManagePasswordsIconMock mock;
  controller()->UpdateIconAndBubbleState(&mock);
  controller()->SavePassword();
  controller()->UpdateIconAndBubbleState(&mock);
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, mock.state());
}

TEST_F(ManagePasswordsUIControllerTest, PasswordBlacklisted) {
  password_manager::StubPasswordManagerClient client;
  password_manager::StubPasswordManagerDriver driver;
  scoped_ptr<password_manager::PasswordFormManager> test_form_manager(
      new password_manager::PasswordFormManager(
          NULL, &client, driver.AsWeakPtr(), test_local_form(), false));
  controller()->OnPasswordSubmitted(test_form_manager.Pass());

  ManagePasswordsIconMock mock;
  controller()->UpdateIconAndBubbleState(&mock);
  controller()->NeverSavePassword();
  controller()->UpdateIconAndBubbleState(&mock);
  EXPECT_EQ(password_manager::ui::BLACKLIST_STATE, mock.state());
}

TEST_F(ManagePasswordsUIControllerTest, QuickNavigations) {
  password_manager::StubPasswordManagerClient client;
  password_manager::StubPasswordManagerDriver driver;
  scoped_ptr<password_manager::PasswordFormManager> test_form_manager(
      new password_manager::PasswordFormManager(
          NULL, &client, driver.AsWeakPtr(), test_local_form(), false));
  controller()->OnPasswordSubmitted(test_form_manager.Pass());
  ManagePasswordsIconMock mock;
  controller()->UpdateIconAndBubbleState(&mock);
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_STATE,
            mock.state());

  // Fake-navigate within a second. We expect the bubble's state to persist
  // if a navigation occurs too quickly for a user to reasonably have been
  // able to interact with the bubble. This happens on `accounts.google.com`,
  // for instance.
  controller()->SetElapsed(
      base::TimeDelta::FromMilliseconds(kQuickNavigationDelayInMS));
  controller()->DidNavigateMainFrame(content::LoadCommittedDetails(),
                                     content::FrameNavigateParams());
  controller()->UpdateIconAndBubbleState(&mock);

  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_STATE,
            mock.state());
}

TEST_F(ManagePasswordsUIControllerTest, SlowNavigations) {
  password_manager::StubPasswordManagerClient client;
  password_manager::StubPasswordManagerDriver driver;
  scoped_ptr<password_manager::PasswordFormManager> test_form_manager(
      new password_manager::PasswordFormManager(
          NULL, &client, driver.AsWeakPtr(), test_local_form(), false));
  controller()->OnPasswordSubmitted(test_form_manager.Pass());
  ManagePasswordsIconMock mock;
  controller()->UpdateIconAndBubbleState(&mock);
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_STATE,
            mock.state());

  // Fake-navigate after a second. We expect the bubble's state to be reset
  // if a navigation occurs after this limit.
  controller()->SetElapsed(
      base::TimeDelta::FromMilliseconds(kSlowNavigationDelayInMS));
  controller()->DidNavigateMainFrame(content::LoadCommittedDetails(),
                                     content::FrameNavigateParams());
  controller()->UpdateIconAndBubbleState(&mock);

  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, mock.state());
}

TEST_F(ManagePasswordsUIControllerTest, PasswordSubmittedToNonWebbyURL) {
  // Navigate to a non-webby URL, then see what happens!
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("chrome://sign-in"));

  password_manager::StubPasswordManagerClient client;
  password_manager::StubPasswordManagerDriver driver;
  scoped_ptr<password_manager::PasswordFormManager> test_form_manager(
      new password_manager::PasswordFormManager(
          NULL, &client, driver.AsWeakPtr(), test_local_form(), false));
  controller()->OnPasswordSubmitted(test_form_manager.Pass());
  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, controller()->state());
  EXPECT_FALSE(controller()->PasswordPendingUserDecision());

  // TODO(mkwst): This should be the value of test_local_form().origin, but
  // it's being masked by the stub implementation of
  // ManagePasswordsUIControllerMock::PendingCredentials.
  EXPECT_EQ(GURL::EmptyGURL(), controller()->origin());

  ManagePasswordsIconMock mock;
  controller()->UpdateIconAndBubbleState(&mock);
  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, mock.state());
}

TEST_F(ManagePasswordsUIControllerTest, BlacklistBlockedAutofill) {
  test_local_form().blacklisted_by_user = true;
  base::string16 kTestUsername = base::ASCIIToUTF16("test_username");
  autofill::PasswordFormMap map;
  map[kTestUsername] = &test_local_form();
  controller()->OnBlacklistBlockedAutofill(map);

  EXPECT_EQ(password_manager::ui::BLACKLIST_STATE, controller()->state());
  EXPECT_FALSE(controller()->PasswordPendingUserDecision());
  EXPECT_EQ(test_local_form().origin, controller()->origin());
  EXPECT_EQ(1u, controller()->best_matches().size());
  ASSERT_EQ(1u, controller()->best_matches().count(kTestUsername));

  // Controller should store a separate copy of the form as it doesn't own it.
  EXPECT_NE(&test_local_form(),
            controller()->best_matches().find(kTestUsername)->second);

  ManagePasswordsIconMock mock;
  controller()->UpdateIconAndBubbleState(&mock);
  EXPECT_EQ(password_manager::ui::BLACKLIST_STATE, mock.state());
}

TEST_F(ManagePasswordsUIControllerTest, ClickedUnblacklist) {
  base::string16 kTestUsername = base::ASCIIToUTF16("test_username");
  autofill::PasswordFormMap map;
  map[kTestUsername] = &test_local_form();
  controller()->OnBlacklistBlockedAutofill(map);
  controller()->UnblacklistSite();

  EXPECT_EQ(password_manager::ui::MANAGE_STATE, controller()->state());
  EXPECT_FALSE(controller()->PasswordPendingUserDecision());
  EXPECT_EQ(test_local_form().origin, controller()->origin());

  ManagePasswordsIconMock mock;
  controller()->UpdateIconAndBubbleState(&mock);
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, mock.state());
}

TEST_F(ManagePasswordsUIControllerTest, UnblacklistedElsewhere) {
  test_local_form().blacklisted_by_user = true;
  base::string16 kTestUsername = base::ASCIIToUTF16("test_username");
  autofill::PasswordFormMap map;
  map[kTestUsername] = &test_local_form();
  controller()->OnBlacklistBlockedAutofill(map);

  password_manager::PasswordStoreChange change(
      password_manager::PasswordStoreChange::REMOVE, test_local_form());
  password_manager::PasswordStoreChangeList list(1, change);
  controller()->OnLoginsChanged(list);

  EXPECT_EQ(password_manager::ui::MANAGE_STATE, controller()->state());
  EXPECT_FALSE(controller()->PasswordPendingUserDecision());
  EXPECT_EQ(test_local_form().origin, controller()->origin());

  ManagePasswordsIconMock mock;
  controller()->UpdateIconAndBubbleState(&mock);
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, mock.state());
}

TEST_F(ManagePasswordsUIControllerTest, BlacklistedElsewhere) {
  base::string16 kTestUsername = base::ASCIIToUTF16("test_username");
  autofill::PasswordFormMap map;
  map[kTestUsername] = &test_local_form();
  controller()->OnPasswordAutofilled(map);

  test_local_form().blacklisted_by_user = true;
  password_manager::PasswordStoreChange change(
      password_manager::PasswordStoreChange::ADD, test_local_form());
  password_manager::PasswordStoreChangeList list(1, change);
  controller()->OnLoginsChanged(list);

  EXPECT_EQ(password_manager::ui::BLACKLIST_STATE, controller()->state());
  EXPECT_FALSE(controller()->PasswordPendingUserDecision());
  EXPECT_EQ(test_local_form().origin, controller()->origin());

  ManagePasswordsIconMock mock;
  controller()->UpdateIconAndBubbleState(&mock);
  EXPECT_EQ(password_manager::ui::BLACKLIST_STATE, mock.state());
}

TEST_F(ManagePasswordsUIControllerTest, AutomaticPasswordSave) {
  password_manager::StubPasswordManagerClient client;
  password_manager::StubPasswordManagerDriver driver;
  scoped_ptr<password_manager::PasswordFormManager> test_form_manager(
      new password_manager::PasswordFormManager(
          NULL, &client, driver.AsWeakPtr(), test_local_form(), false));

  controller()->OnAutomaticPasswordSave(test_form_manager.Pass());
  EXPECT_EQ(password_manager::ui::CONFIRMATION_STATE, controller()->state());

  controller()->OnBubbleHidden();
  ManagePasswordsIconMock mock;
  controller()->UpdateIconAndBubbleState(&mock);
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, mock.state());
}

TEST_F(ManagePasswordsUIControllerTest, ChooseCredentialLocal) {
  ScopedVector<autofill::PasswordForm> local_credentials;
  local_credentials.push_back(new autofill::PasswordForm(test_local_form()));
  ScopedVector<autofill::PasswordForm> federated_credentials;
  GURL origin("http://example.com");
  EXPECT_TRUE(controller()->OnChooseCredentials(
      local_credentials.Pass(), federated_credentials.Pass(), origin,
      base::Bind(&ManagePasswordsUIControllerTest::CredentialCallback,
                 base::Unretained(this))));
  EXPECT_EQ(password_manager::ui::CREDENTIAL_REQUEST_STATE,
            controller()->state());
  EXPECT_FALSE(controller()->PasswordPendingUserDecision());
  EXPECT_EQ(origin, controller()->origin());
  EXPECT_EQ(autofill::ConstPasswordFormMap(), controller()->best_matches());

  ManagePasswordsIconMock mock;
  controller()->UpdateIconAndBubbleState(&mock);
  EXPECT_EQ(password_manager::ui::CREDENTIAL_REQUEST_STATE, mock.state());

  controller()->ManagePasswordsUIController::ChooseCredential(
      test_local_form(),
      password_manager::CredentialType::CREDENTIAL_TYPE_LOCAL);
  controller()->OnBubbleHidden();
  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, controller()->state());
  ASSERT_TRUE(credential_info());
  EXPECT_EQ(test_local_form().username_value, credential_info()->id);
  EXPECT_EQ(test_local_form().password_value, credential_info()->password);
  EXPECT_TRUE(credential_info()->federation.is_empty());
  EXPECT_EQ(password_manager::CredentialType::CREDENTIAL_TYPE_LOCAL,
            credential_info()->type);
}

TEST_F(ManagePasswordsUIControllerTest, ChooseCredentialLocalButFederated) {
  ScopedVector<autofill::PasswordForm> local_credentials;
  local_credentials.push_back(
      new autofill::PasswordForm(test_federated_form()));
  ScopedVector<autofill::PasswordForm> federated_credentials;
  GURL origin("http://example.com");
  EXPECT_TRUE(controller()->OnChooseCredentials(
      local_credentials.Pass(), federated_credentials.Pass(), origin,
      base::Bind(&ManagePasswordsUIControllerTest::CredentialCallback,
                 base::Unretained(this))));
  EXPECT_EQ(password_manager::ui::CREDENTIAL_REQUEST_STATE,
            controller()->state());
  EXPECT_FALSE(controller()->PasswordPendingUserDecision());
  EXPECT_EQ(origin, controller()->origin());
  EXPECT_EQ(autofill::ConstPasswordFormMap(), controller()->best_matches());

  ManagePasswordsIconMock mock;
  controller()->UpdateIconAndBubbleState(&mock);
  EXPECT_EQ(password_manager::ui::CREDENTIAL_REQUEST_STATE, mock.state());

  controller()->ManagePasswordsUIController::ChooseCredential(
      test_federated_form(),
      password_manager::CredentialType::CREDENTIAL_TYPE_LOCAL);
  controller()->OnBubbleHidden();
  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, controller()->state());
  ASSERT_TRUE(credential_info());
  EXPECT_EQ(test_federated_form().username_value, credential_info()->id);
  EXPECT_EQ(test_federated_form().federation_url,
            credential_info()->federation);
  EXPECT_TRUE(credential_info()->password.empty());
  EXPECT_EQ(password_manager::CredentialType::CREDENTIAL_TYPE_FEDERATED,
            credential_info()->type);
}

TEST_F(ManagePasswordsUIControllerTest, ChooseCredentialFederated) {
  ScopedVector<autofill::PasswordForm> local_credentials;
  ScopedVector<autofill::PasswordForm> federated_credentials;
  federated_credentials.push_back(
      new autofill::PasswordForm(test_local_form()));
  GURL origin("http://example.com");
  EXPECT_TRUE(controller()->OnChooseCredentials(
      local_credentials.Pass(), federated_credentials.Pass(), origin,
      base::Bind(&ManagePasswordsUIControllerTest::CredentialCallback,
                 base::Unretained(this))));
  EXPECT_EQ(password_manager::ui::CREDENTIAL_REQUEST_STATE,
            controller()->state());
  EXPECT_FALSE(controller()->PasswordPendingUserDecision());
  EXPECT_EQ(autofill::ConstPasswordFormMap(), controller()->best_matches());
  EXPECT_EQ(origin, controller()->origin());

  ManagePasswordsIconMock mock;
  controller()->UpdateIconAndBubbleState(&mock);
  EXPECT_EQ(password_manager::ui::CREDENTIAL_REQUEST_STATE, mock.state());

  controller()->ManagePasswordsUIController::ChooseCredential(
     test_local_form(),
      password_manager::CredentialType::CREDENTIAL_TYPE_FEDERATED);
  controller()->OnBubbleHidden();
  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, controller()->state());
  ASSERT_TRUE(credential_info());
  EXPECT_EQ(test_local_form().username_value, credential_info()->id);
  EXPECT_TRUE(credential_info()->password.empty());
  EXPECT_EQ(password_manager::CredentialType::CREDENTIAL_TYPE_FEDERATED,
            credential_info()->type);
}

TEST_F(ManagePasswordsUIControllerTest, ChooseCredentialCancel) {
  ScopedVector<autofill::PasswordForm> local_credentials;
  local_credentials.push_back(new autofill::PasswordForm(test_local_form()));
  ScopedVector<autofill::PasswordForm> federated_credentials;
  GURL origin("http://example.com");
  EXPECT_TRUE(controller()->OnChooseCredentials(
      local_credentials.Pass(), federated_credentials.Pass(), origin,
      base::Bind(&ManagePasswordsUIControllerTest::CredentialCallback,
                 base::Unretained(this))));
  EXPECT_EQ(password_manager::ui::CREDENTIAL_REQUEST_STATE,
            controller()->state());
  EXPECT_EQ(origin, controller()->origin());
  controller()->ManagePasswordsUIController::ChooseCredential(
      test_local_form(),
      password_manager::CredentialType::CREDENTIAL_TYPE_EMPTY);
  controller()->OnBubbleHidden();
  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, controller()->state());
  ASSERT_TRUE(credential_info());
  EXPECT_TRUE(credential_info()->federation.is_empty());
  EXPECT_TRUE(credential_info()->password.empty());
  EXPECT_EQ(password_manager::CredentialType::CREDENTIAL_TYPE_EMPTY,
            credential_info()->type);
}

TEST_F(ManagePasswordsUIControllerTest, AutoSignin) {
  ScopedVector<autofill::PasswordForm> local_credentials;
  local_credentials.push_back(new autofill::PasswordForm(test_local_form()));
  controller()->OnAutoSignin(local_credentials.Pass());
  EXPECT_EQ(password_manager::ui::AUTO_SIGNIN_STATE, controller()->state());
  EXPECT_EQ(test_local_form().origin, controller()->origin());
  ASSERT_FALSE(controller()->local_credentials_forms().empty());
  EXPECT_EQ(test_local_form(), *controller()->local_credentials_forms()[0]);
  ManagePasswordsIconMock mock;
  controller()->UpdateIconAndBubbleState(&mock);
  EXPECT_EQ(password_manager::ui::AUTO_SIGNIN_STATE, mock.state());

  controller()->OnBubbleHidden();
  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, controller()->state());
  controller()->UpdateIconAndBubbleState(&mock);
  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, mock.state());
}

TEST_F(ManagePasswordsUIControllerTest, InactiveOnPSLMatched) {
  base::string16 kTestUsername = base::ASCIIToUTF16("test_username");
  autofill::PasswordFormMap map;
  autofill::PasswordForm psl_matched_test_form = test_local_form();
  psl_matched_test_form.original_signon_realm = "http://pslmatched.example.com";
  map[kTestUsername] = &psl_matched_test_form;
  controller()->OnPasswordAutofilled(map);

  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, controller()->state());
}
