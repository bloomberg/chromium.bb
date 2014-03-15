// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_error_notifier_ash.h"

#include "ash/test/ash_test_base.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/notifications/notification.h"
#include "chrome/browser/notifications/notification_ui_manager.h"
#include "chrome/browser/signin/fake_auth_status_provider.h"
#include "chrome/browser/signin/fake_signin_manager.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_error_controller.h"
#include "chrome/browser/signin/signin_error_notifier_factory_ash.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/ash/test_views_delegate_with_parent.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/message_center/notification.h"

#if !defined(OS_CHROMEOS)
#include "chrome/browser/ui/ash/ash_util.h"
#include "ui/aura/test/test_screen.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/screen_type_delegate.h"
#endif

namespace ash {
namespace test {

namespace {

static const char kTestAccountId[] = "testuser@test.com";

// Notification ID corresponding to kProfileSigninNotificationId +
// kTestAccountId.
static const std::string kNotificationId =
    "chrome://settings/signin/testuser@test.com";
}

#if !defined(OS_CHROMEOS)
class ScreenTypeDelegateDesktop : public gfx::ScreenTypeDelegate {
 public:
  ScreenTypeDelegateDesktop() {}
  virtual gfx::ScreenType GetScreenTypeForNativeView(
      gfx::NativeView view) OVERRIDE {
    return chrome::IsNativeViewInAsh(view) ?
        gfx::SCREEN_TYPE_ALTERNATE :
        gfx::SCREEN_TYPE_NATIVE;
  }
 private:
  DISALLOW_COPY_AND_ASSIGN(ScreenTypeDelegateDesktop);
};
#endif

class SigninErrorNotifierTest : public AshTestBase {
 public:
  virtual void SetUp() OVERRIDE {
    views::ViewsDelegate::views_delegate = &views_delegate_;

    // Create a signed-in profile.
    profile_.reset(new TestingProfile());
    profile_->set_profile_name(kTestAccountId);

    SigninManagerFactory::GetInstance()->SetTestingFactoryAndUse(
            profile_.get(), FakeSigninManagerBase::Build);

    profile_manager_.reset(
        new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(profile_manager_->SetUp());

    TestingBrowserProcess::GetGlobal();
    AshTestBase::SetUp();

    // Set up screen for Windows.
#if !defined(OS_CHROMEOS)
    aura::TestScreen* test_screen = aura::TestScreen::Create();
    gfx::Screen::SetScreenInstance(gfx::SCREEN_TYPE_NATIVE, test_screen);
    gfx::Screen::SetScreenTypeDelegate(new ScreenTypeDelegateDesktop);
#endif

    error_controller_ =
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile_.get())->
            signin_error_controller();
    SigninErrorNotifierFactory::GetForProfile(profile_.get());
    notification_ui_manager_ = g_browser_process->notification_ui_manager();
  }

  virtual void TearDown() OVERRIDE {
    profile_manager_.reset();

    AshTestBase::TearDown();
  }

 protected:
  void GetMessage(base::string16* message) {
    const Notification* notification =
        g_browser_process->notification_ui_manager()->FindById(kNotificationId);
    ASSERT_FALSE(notification == NULL);
    *message = notification->message();
  }

  scoped_ptr<TestingProfileManager> profile_manager_;
  scoped_ptr<TestingProfile> profile_;
  SigninErrorController* error_controller_;
  NotificationUIManager* notification_ui_manager_;
  TestViewsDelegateWithParent views_delegate_;
};

TEST_F(SigninErrorNotifierTest, NoErrorAuthStatusProviders) {
  ASSERT_FALSE(notification_ui_manager_->FindById(kNotificationId));
  {
    // Add a provider (removes itself on exiting this scope).
    FakeAuthStatusProvider provider(error_controller_);
    ASSERT_FALSE(notification_ui_manager_->FindById(kNotificationId));
  }
  ASSERT_FALSE(notification_ui_manager_->FindById(kNotificationId));
}

TEST_F(SigninErrorNotifierTest, ErrorAuthStatusProvider) {
  {
    FakeAuthStatusProvider provider(error_controller_);
    ASSERT_FALSE(notification_ui_manager_->FindById(kNotificationId));
    {
      FakeAuthStatusProvider error_provider(error_controller_);
      LOG(ERROR) << "Setting auth error";
      error_provider.SetAuthError(kTestAccountId, GoogleServiceAuthError(
          GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
      ASSERT_TRUE(notification_ui_manager_->FindById(kNotificationId));
    }
    // error_provider is removed now that we've left that scope.
    ASSERT_FALSE(notification_ui_manager_->FindById(kNotificationId));
  }
  // All providers should be removed now.
  ASSERT_FALSE(notification_ui_manager_->FindById(kNotificationId));
}

TEST_F(SigninErrorNotifierTest, AuthStatusProviderErrorTransition) {
  {
    FakeAuthStatusProvider provider0(error_controller_);
    FakeAuthStatusProvider provider1(error_controller_);
    provider0.SetAuthError(
        kTestAccountId,
        GoogleServiceAuthError(
            GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
    ASSERT_TRUE(notification_ui_manager_->FindById(kNotificationId));

    base::string16 message;
    GetMessage(&message);
    ASSERT_FALSE(message.empty());

    // Now set another auth error and clear the original.
    provider1.SetAuthError(
        kTestAccountId,
        GoogleServiceAuthError(
            GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE));
    provider0.SetAuthError(
        kTestAccountId, GoogleServiceAuthError::AuthErrorNone());

    ASSERT_TRUE(notification_ui_manager_->FindById(kNotificationId));

    base::string16 new_message;
    GetMessage(&new_message);
    ASSERT_FALSE(new_message.empty());

    ASSERT_NE(new_message, message);

    provider1.SetAuthError(
        kTestAccountId, GoogleServiceAuthError::AuthErrorNone());
    ASSERT_FALSE(notification_ui_manager_->FindById(kNotificationId));
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
    { GoogleServiceAuthError::SERVICE_UNAVAILABLE, true },
    { GoogleServiceAuthError::TWO_FACTOR, true },
    { GoogleServiceAuthError::REQUEST_CANCELED, true },
    { GoogleServiceAuthError::HOSTED_NOT_ALLOWED, true },
    { GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE, true },
    { GoogleServiceAuthError::SERVICE_ERROR, true },
  };
  COMPILE_ASSERT(ARRAYSIZE_UNSAFE(table) == GoogleServiceAuthError::NUM_STATES,
      kTable_size_does_not_match_number_of_auth_error_types);

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(table); ++i) {
    FakeAuthStatusProvider provider(error_controller_);
    provider.SetAuthError(kTestAccountId,
                          GoogleServiceAuthError(table[i].error_state));
    const Notification* notification = notification_ui_manager_->
        FindById(kNotificationId);
    ASSERT_EQ(table[i].is_error, notification != NULL);
    if (table[i].is_error) {
      EXPECT_FALSE(notification->title().empty());
      EXPECT_FALSE(notification->message().empty());
      EXPECT_EQ((size_t)1, notification->buttons().size());
    }
  }
}

}  // namespace test
}  // namespace ash
