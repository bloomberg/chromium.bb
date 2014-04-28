// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram_samples.h"
#include "base/prefs/pref_service.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/statistics_delta_reader.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_model.h"
#include "chrome/browser/ui/passwords/manage_passwords_bubble_ui_controller_mock.h"
#include "chrome/browser/ui/passwords/manage_passwords_icon.h"
#include "chrome/browser/ui/passwords/manage_passwords_icon_mock.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/common/password_form.h"
#include "components/password_manager/core/browser/mock_password_manager_driver.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_driver.h"
#include "components/password_manager/core/browser/stub_password_manager_client.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "content/public/test/web_contents_tester.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class ManagePasswordsBubbleUIControllerTest : public testing::Test {
 public:
  ManagePasswordsBubbleUIControllerTest()
      : test_web_contents_(
            content::WebContentsTester::CreateTestWebContents(&profile_,
                                                              NULL)) {}

  virtual void SetUp() OVERRIDE {
    // Create the test UIController here so that it's bound to
    // |test_web_contents_|, and will be retrieved correctly via
    // ManagePasswordsBubbleUIController::FromWebContents in |controller()|.
    new ManagePasswordsBubbleUIControllerMock(test_web_contents_.get());

    test_form_.origin = GURL("http://example.com");
  }

  autofill::PasswordForm& test_form() { return test_form_; }

  ManagePasswordsBubbleUIControllerMock* controller() {
    return static_cast<ManagePasswordsBubbleUIControllerMock*>(
        ManagePasswordsBubbleUIController::FromWebContents(
            test_web_contents_.get()));
  }

 private:
  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
  scoped_ptr<content::WebContents> test_web_contents_;

  autofill::PasswordForm test_form_;
};

TEST_F(ManagePasswordsBubbleUIControllerTest, DefaultState) {
  EXPECT_FALSE(controller()->autofill_blocked());
  EXPECT_FALSE(controller()->manage_passwords_bubble_needs_showing());
  EXPECT_FALSE(controller()->manage_passwords_icon_to_be_shown());
  EXPECT_FALSE(controller()->never_saved_password());
  EXPECT_FALSE(controller()->password_to_be_saved());
  EXPECT_FALSE(controller()->saved_password());

  EXPECT_EQ(GURL::EmptyGURL(), controller()->origin());

  ManagePasswordsIconMock mock;
  controller()->UpdateIconAndBubbleState(&mock);
  EXPECT_EQ(ManagePasswordsIcon::INACTIVE_STATE, mock.state());
  EXPECT_EQ(0, mock.bubble_shown_count());
}

TEST_F(ManagePasswordsBubbleUIControllerTest, PasswordAutofilled) {
  base::string16 kTestUsername = base::ASCIIToUTF16("test_username");
  autofill::PasswordFormMap map;
  map[kTestUsername] = &test_form();
  controller()->OnPasswordAutofilled(map);

  EXPECT_TRUE(controller()->manage_passwords_icon_to_be_shown());

  EXPECT_FALSE(controller()->autofill_blocked());
  EXPECT_FALSE(controller()->manage_passwords_bubble_needs_showing());
  EXPECT_FALSE(controller()->never_saved_password());
  EXPECT_FALSE(controller()->password_to_be_saved());
  EXPECT_FALSE(controller()->saved_password());

  EXPECT_EQ(test_form().origin, controller()->origin());

  ManagePasswordsIconMock mock;
  controller()->UpdateIconAndBubbleState(&mock);
  EXPECT_EQ(ManagePasswordsIcon::MANAGE_STATE, mock.state());
  EXPECT_EQ(0, mock.bubble_shown_count());
}

TEST_F(ManagePasswordsBubbleUIControllerTest, PasswordSubmitted) {
  password_manager::StubPasswordManagerClient client;
  password_manager::MockPasswordManagerDriver driver;
  password_manager::PasswordFormManager* test_form_manager =
      new password_manager::PasswordFormManager(
          NULL, &client, &driver, test_form(), false);
  controller()->OnPasswordSubmitted(test_form_manager);

  EXPECT_TRUE(controller()->manage_passwords_bubble_needs_showing());
  EXPECT_TRUE(controller()->manage_passwords_icon_to_be_shown());
  EXPECT_TRUE(controller()->password_to_be_saved());

  EXPECT_FALSE(controller()->autofill_blocked());
  EXPECT_FALSE(controller()->never_saved_password());
  EXPECT_FALSE(controller()->saved_password());

  // TODO(mkwst): This should be the value of test_form()->origin, but
  // it's being masked by the stub implementation of
  // ManagePasswordsBubbleUIControllerMock::PendingCredentials.
  EXPECT_EQ(GURL::EmptyGURL(), controller()->origin());

  ManagePasswordsIconMock mock;
  controller()->UpdateIconAndBubbleState(&mock);
  EXPECT_EQ(ManagePasswordsIcon::PENDING_STATE, mock.state());
  EXPECT_EQ(1, mock.bubble_shown_count());
}

TEST_F(ManagePasswordsBubbleUIControllerTest, BlacklistBlockedAutofill) {
  controller()->OnBlacklistBlockedAutofill();

  EXPECT_TRUE(controller()->autofill_blocked());
  EXPECT_TRUE(controller()->manage_passwords_icon_to_be_shown());

  EXPECT_FALSE(controller()->manage_passwords_bubble_needs_showing());
  EXPECT_FALSE(controller()->never_saved_password());
  EXPECT_FALSE(controller()->password_to_be_saved());
  EXPECT_FALSE(controller()->saved_password());

  EXPECT_EQ(GURL::EmptyGURL(), controller()->origin());

  ManagePasswordsIconMock mock;
  controller()->UpdateIconAndBubbleState(&mock);
  EXPECT_EQ(ManagePasswordsIcon::BLACKLISTED_STATE, mock.state());
  EXPECT_EQ(0, mock.bubble_shown_count());
}
