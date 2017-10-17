// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_error_notifier_ash.h"

#include <stddef.h>

#include <memory>

#include "base/macros.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/login/users/mock_user_manager.h"
#include "chrome/browser/chromeos/login/users/scoped_user_manager_enabler.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/signin/fake_signin_manager_builder.h"
#include "chrome/browser/signin/signin_error_controller_factory.h"
#include "chrome/browser/signin/signin_error_notifier_factory_ash.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/test/base/browser_with_test_window_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/signin/core/browser/fake_auth_status_provider.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/notification.h"

namespace {

static const char kTestAccountId[] = "testuser@test.com";

// Notification ID corresponding to kProfileSigninNotificationId +
// kTestAccountId.
static const char kNotificationId[] =
    "chrome://settings/signin/testuser@test.com";

class SigninErrorNotifierTest : public BrowserWithTestWindowTest {
 public:
  void SetUp() override {
    BrowserWithTestWindowTest::SetUp();
    profile_manager_.reset(
        new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(profile_manager_->SetUp());

    mock_user_manager_ = new chromeos::MockUserManager();
    user_manager_enabler_.reset(
        new chromeos::ScopedUserManagerEnabler(mock_user_manager_));

    error_controller_ =
        SigninErrorControllerFactory::GetForProfile(GetProfile());
    SigninErrorNotifierFactory::GetForProfile(GetProfile());
    notification_ui_manager_ = g_browser_process->notification_ui_manager();
  }

  void TearDown() override {
    profile_manager_.reset();
    BrowserWithTestWindowTest::TearDown();
  }

  TestingProfile* CreateProfile() override {
    // Create a signed-in profile.
    TestingProfile::Builder builder;
    builder.AddTestingFactory(SigninManagerFactory::GetInstance(),
                              BuildFakeSigninManagerBase);
    std::unique_ptr<TestingProfile> profile = builder.Build();
    profile->set_profile_name(kTestAccountId);
    return profile.release();
  }

 protected:
  void GetMessage(base::string16* message) {
    const message_center::Notification* notification =
        g_browser_process->notification_ui_manager()->FindById(
            kNotificationId, NotificationUIManager::GetProfileID(GetProfile()));
    ASSERT_FALSE(notification == NULL);
    *message = notification->message();
  }

  std::unique_ptr<TestingProfileManager> profile_manager_;
  SigninErrorController* error_controller_;
  NotificationUIManager* notification_ui_manager_;
  chromeos::MockUserManager* mock_user_manager_;  // Not owned.
  std::unique_ptr<chromeos::ScopedUserManagerEnabler> user_manager_enabler_;
};

TEST_F(SigninErrorNotifierTest, NoErrorAuthStatusProviders) {
  ASSERT_FALSE(notification_ui_manager_->FindById(
      kNotificationId, NotificationUIManager::GetProfileID(GetProfile())));
  {
    // Add a provider (removes itself on exiting this scope).
    FakeAuthStatusProvider provider(error_controller_);
    ASSERT_FALSE(notification_ui_manager_->FindById(
        kNotificationId, NotificationUIManager::GetProfileID(GetProfile())));
  }
  ASSERT_FALSE(notification_ui_manager_->FindById(
      kNotificationId, NotificationUIManager::GetProfileID(GetProfile())));
}

TEST_F(SigninErrorNotifierTest, ErrorAuthStatusProvider) {
  {
    FakeAuthStatusProvider provider(error_controller_);
    ASSERT_FALSE(notification_ui_manager_->FindById(
        kNotificationId, NotificationUIManager::GetProfileID(GetProfile())));
    {
      FakeAuthStatusProvider error_provider(error_controller_);
      error_provider.SetAuthError(
          kTestAccountId,
          GoogleServiceAuthError(
              GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
      ASSERT_TRUE(notification_ui_manager_->FindById(
          kNotificationId, NotificationUIManager::GetProfileID(GetProfile())));
    }
    // error_provider is removed now that we've left that scope.
    ASSERT_FALSE(notification_ui_manager_->FindById(
        kNotificationId, NotificationUIManager::GetProfileID(GetProfile())));
  }
  // All providers should be removed now.
  ASSERT_FALSE(notification_ui_manager_->FindById(
      kNotificationId, NotificationUIManager::GetProfileID(GetProfile())));
}

TEST_F(SigninErrorNotifierTest, AuthStatusProviderErrorTransition) {
  {
    FakeAuthStatusProvider provider0(error_controller_);
    FakeAuthStatusProvider provider1(error_controller_);
    provider0.SetAuthError(
        kTestAccountId,
        GoogleServiceAuthError(
            GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
    ASSERT_TRUE(notification_ui_manager_->FindById(
        kNotificationId, NotificationUIManager::GetProfileID(GetProfile())));

    base::string16 message;
    GetMessage(&message);
    ASSERT_FALSE(message.empty());

    // Now set another auth error and clear the original.
    provider1.SetAuthError(
        kTestAccountId,
        GoogleServiceAuthError(
            GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE));
    provider0.SetAuthError(
        kTestAccountId,
        GoogleServiceAuthError::AuthErrorNone());

    ASSERT_TRUE(notification_ui_manager_->FindById(
        kNotificationId, NotificationUIManager::GetProfileID(GetProfile())));

    base::string16 new_message;
    GetMessage(&new_message);
    ASSERT_FALSE(new_message.empty());

    ASSERT_NE(new_message, message);

    provider1.SetAuthError(
        kTestAccountId, GoogleServiceAuthError::AuthErrorNone());
    ASSERT_FALSE(notification_ui_manager_->FindById(
        kNotificationId, NotificationUIManager::GetProfileID(GetProfile())));
  }
}

// Verify that SigninErrorNotifier ignores certain errors.
TEST_F(SigninErrorNotifierTest, AuthStatusEnumerateAllErrors) {
  typedef struct {
    GoogleServiceAuthError::State error_state;
    bool is_error;
  } ErrorTableEntry;

  ErrorTableEntry table[] = {
    { GoogleServiceAuthError::NONE, false },
    { GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS, true },
    { GoogleServiceAuthError::USER_NOT_SIGNED_UP, true },
    { GoogleServiceAuthError::CONNECTION_FAILED, false },
    { GoogleServiceAuthError::CAPTCHA_REQUIRED, true },
    { GoogleServiceAuthError::ACCOUNT_DELETED, true },
    { GoogleServiceAuthError::ACCOUNT_DISABLED, true },
    { GoogleServiceAuthError::SERVICE_UNAVAILABLE, false },
    { GoogleServiceAuthError::TWO_FACTOR, true },
    { GoogleServiceAuthError::REQUEST_CANCELED, false },
    { GoogleServiceAuthError::HOSTED_NOT_ALLOWED_DEPRECATED, false },
    { GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE, true },
    { GoogleServiceAuthError::SERVICE_ERROR, true },
    { GoogleServiceAuthError::WEB_LOGIN_REQUIRED, true },
  };
  static_assert(arraysize(table) == GoogleServiceAuthError::NUM_STATES,
      "table size should match number of auth error types");

  for (size_t i = 0; i < arraysize(table); ++i) {
    if (GoogleServiceAuthError::IsDeprecated(table[i].error_state))
      continue;
    FakeAuthStatusProvider provider(error_controller_);
    provider.SetAuthError(kTestAccountId,
                          GoogleServiceAuthError(table[i].error_state));
    const message_center::Notification* notification =
        notification_ui_manager_->FindById(
            kNotificationId, NotificationUIManager::GetProfileID(GetProfile()));
    ASSERT_EQ(table[i].is_error, notification != NULL);
    if (table[i].is_error) {
      EXPECT_FALSE(notification->title().empty());
      EXPECT_FALSE(notification->message().empty());
      EXPECT_EQ((size_t)1, notification->buttons().size());
    }
  }
}

}  // namespace
