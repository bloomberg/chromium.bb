// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram_samples.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/statistics_delta_reader.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/passwords/manage_passwords_icon.h"
#include "chrome/browser/ui/passwords/manage_passwords_icon_mock.h"
#include "chrome/browser/ui/passwords/manage_passwords_ui_controller_mock.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/mock_password_manager_driver.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "components/password_manager/core/common/password_manager_ui.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class ManagePasswordsUIControllerTest : public ChromeRenderViewHostTestHarness {
 public:
  ManagePasswordsUIControllerTest() {}

  virtual void SetUp() OVERRIDE {
    ChromeRenderViewHostTestHarness::SetUp();

    // Create the test UIController here so that it's bound to
    // |test_web_contents_|, and will be retrieved correctly via
    // ManagePasswordsUIController::FromWebContents in |controller()|.
    new ManagePasswordsUIControllerMock(web_contents());

    test_form_.origin = GURL("http://example.com");

    // We need to be on a "webby" URL for most tests.
    content::WebContentsTester::For(web_contents())
        ->NavigateAndCommit(GURL("http://example.com"));
  }

  autofill::PasswordForm& test_form() { return test_form_; }

  ManagePasswordsUIControllerMock* controller() {
    return static_cast<ManagePasswordsUIControllerMock*>(
        ManagePasswordsUIController::FromWebContents(web_contents()));
  }

 private:
  autofill::PasswordForm test_form_;
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
  map[kTestUsername] = &test_form();
  controller()->OnPasswordAutofilled(map);

  EXPECT_EQ(password_manager::ui::MANAGE_STATE, controller()->state());
  EXPECT_FALSE(controller()->PasswordPendingUserDecision());
  EXPECT_EQ(test_form().origin, controller()->origin());

  ManagePasswordsIconMock mock;
  controller()->UpdateIconAndBubbleState(&mock);
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, mock.state());
}

TEST_F(ManagePasswordsUIControllerTest, PasswordSubmitted) {
  password_manager::StubPasswordManagerClient client;
  password_manager::MockPasswordManagerDriver driver;
  password_manager::PasswordFormManager* test_form_manager =
      new password_manager::PasswordFormManager(
          NULL, &client, &driver, test_form(), false);
  controller()->OnPasswordSubmitted(test_form_manager);
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_AND_BUBBLE_STATE,
            controller()->state());
  EXPECT_TRUE(controller()->PasswordPendingUserDecision());

  // TODO(mkwst): This should be the value of test_form().origin, but
  // it's being masked by the stub implementation of
  // ManagePasswordsUIControllerMock::PendingCredentials.
  EXPECT_EQ(GURL::EmptyGURL(), controller()->origin());

  ManagePasswordsIconMock mock;
  controller()->UpdateIconAndBubbleState(&mock);
  EXPECT_EQ(password_manager::ui::PENDING_PASSWORD_STATE, mock.state());
}

TEST_F(ManagePasswordsUIControllerTest, PasswordSubmittedToNonWebbyURL) {
  // Navigate to a non-webby URL, then see what happens!
  content::WebContentsTester::For(web_contents())
      ->NavigateAndCommit(GURL("chrome://sign-in"));

  password_manager::StubPasswordManagerClient client;
  password_manager::MockPasswordManagerDriver driver;
  password_manager::PasswordFormManager* test_form_manager =
      new password_manager::PasswordFormManager(
          NULL, &client, &driver, test_form(), false);
  controller()->OnPasswordSubmitted(test_form_manager);
  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, controller()->state());
  EXPECT_FALSE(controller()->PasswordPendingUserDecision());

  // TODO(mkwst): This should be the value of test_form().origin, but
  // it's being masked by the stub implementation of
  // ManagePasswordsUIControllerMock::PendingCredentials.
  EXPECT_EQ(GURL::EmptyGURL(), controller()->origin());

  ManagePasswordsIconMock mock;
  controller()->UpdateIconAndBubbleState(&mock);
  EXPECT_EQ(password_manager::ui::INACTIVE_STATE, mock.state());
}

TEST_F(ManagePasswordsUIControllerTest, BlacklistBlockedAutofill) {
  test_form().blacklisted_by_user = true;
  base::string16 kTestUsername = base::ASCIIToUTF16("test_username");
  autofill::PasswordFormMap map;
  map[kTestUsername] = &test_form();
  controller()->OnBlacklistBlockedAutofill(map);

  EXPECT_EQ(password_manager::ui::BLACKLIST_STATE, controller()->state());
  EXPECT_FALSE(controller()->PasswordPendingUserDecision());
  EXPECT_EQ(test_form().origin, controller()->origin());

  ManagePasswordsIconMock mock;
  controller()->UpdateIconAndBubbleState(&mock);
  EXPECT_EQ(password_manager::ui::BLACKLIST_STATE, mock.state());
}

TEST_F(ManagePasswordsUIControllerTest, ClickedUnblacklist) {
  base::string16 kTestUsername = base::ASCIIToUTF16("test_username");
  autofill::PasswordFormMap map;
  map[kTestUsername] = &test_form();
  controller()->OnBlacklistBlockedAutofill(map);
  controller()->UnblacklistSite();

  EXPECT_EQ(password_manager::ui::MANAGE_STATE, controller()->state());
  EXPECT_FALSE(controller()->PasswordPendingUserDecision());
  EXPECT_EQ(test_form().origin, controller()->origin());

  ManagePasswordsIconMock mock;
  controller()->UpdateIconAndBubbleState(&mock);
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, mock.state());
}

TEST_F(ManagePasswordsUIControllerTest, UnblacklistedElsewhere) {
  test_form().blacklisted_by_user = true;
  base::string16 kTestUsername = base::ASCIIToUTF16("test_username");
  autofill::PasswordFormMap map;
  map[kTestUsername] = &test_form();
  controller()->OnBlacklistBlockedAutofill(map);

  password_manager::PasswordStoreChange change(
      password_manager::PasswordStoreChange::REMOVE, test_form());
  password_manager::PasswordStoreChangeList list(1, change);
  controller()->OnLoginsChanged(list);

  EXPECT_EQ(password_manager::ui::MANAGE_STATE, controller()->state());
  EXPECT_FALSE(controller()->PasswordPendingUserDecision());
  EXPECT_EQ(test_form().origin, controller()->origin());

  ManagePasswordsIconMock mock;
  controller()->UpdateIconAndBubbleState(&mock);
  EXPECT_EQ(password_manager::ui::MANAGE_STATE, mock.state());
}

TEST_F(ManagePasswordsUIControllerTest, BlacklistedElsewhere) {
  base::string16 kTestUsername = base::ASCIIToUTF16("test_username");
  autofill::PasswordFormMap map;
  map[kTestUsername] = &test_form();
  controller()->OnPasswordAutofilled(map);

  test_form().blacklisted_by_user = true;
  password_manager::PasswordStoreChange change(
      password_manager::PasswordStoreChange::ADD, test_form());
  password_manager::PasswordStoreChangeList list(1, change);
  controller()->OnLoginsChanged(list);

  EXPECT_EQ(password_manager::ui::BLACKLIST_STATE, controller()->state());
  EXPECT_FALSE(controller()->PasswordPendingUserDecision());
  EXPECT_EQ(test_form().origin, controller()->origin());

  ManagePasswordsIconMock mock;
  controller()->UpdateIconAndBubbleState(&mock);
  EXPECT_EQ(password_manager::ui::BLACKLIST_STATE, mock.state());
}

