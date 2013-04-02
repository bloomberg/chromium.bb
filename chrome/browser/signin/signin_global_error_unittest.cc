// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_global_error.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/signin/fake_auth_status_provider.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_manager_fake.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

class SigninGlobalErrorTest : public testing::Test {
 public:
  virtual void SetUp() OVERRIDE {
    // Create a signed-in profile.
    profile_.reset(new TestingProfile());

    SigninManagerFactory::GetInstance()->SetTestingFactoryAndUse(
        profile_.get(), FakeSigninManager::Build);
    SigninManager* manager =
        SigninManagerFactory::GetForProfile(profile_.get());
      manager->SetAuthenticatedUsername("testuser@test.com");
    global_error_ = manager->signin_global_error();
  }

  scoped_ptr<TestingProfile> profile_;
  SigninGlobalError* global_error_;
};

TEST_F(SigninGlobalErrorTest, NoAuthStatusProviders) {
  ASSERT_FALSE(global_error_->HasMenuItem());
}

TEST_F(SigninGlobalErrorTest, NoErrorAuthStatusProviders) {
  {
    // Add a provider (removes itself on exiting this scope).
    FakeAuthStatusProvider provider(global_error_);
    ASSERT_FALSE(global_error_->HasMenuItem());
  }
  ASSERT_FALSE(global_error_->HasMenuItem());
}

TEST_F(SigninGlobalErrorTest, ErrorAuthStatusProvider) {
  {
    FakeAuthStatusProvider provider(global_error_);
    ASSERT_FALSE(global_error_->HasMenuItem());
    {
      FakeAuthStatusProvider error_provider(global_error_);
      error_provider.SetAuthError(GoogleServiceAuthError(
          GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
      ASSERT_TRUE(global_error_->HasMenuItem());
    }
    // error_provider is removed now that we've left that scope.
    ASSERT_FALSE(global_error_->HasMenuItem());
  }
  // All providers should be removed now.
  ASSERT_FALSE(global_error_->HasMenuItem());
}

TEST_F(SigninGlobalErrorTest, AuthStatusProviderErrorTransition) {
  {
    FakeAuthStatusProvider provider0(global_error_);
    FakeAuthStatusProvider provider1(global_error_);
    ASSERT_FALSE(global_error_->HasMenuItem());
    provider0.SetAuthError(
        GoogleServiceAuthError(
            GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
    ASSERT_TRUE(global_error_->HasMenuItem());
    provider1.SetAuthError(
        GoogleServiceAuthError(GoogleServiceAuthError::ACCOUNT_DISABLED));
    ASSERT_TRUE(global_error_->HasMenuItem());

    // Now resolve the auth errors - the menu item should go away.
    provider0.SetAuthError(GoogleServiceAuthError::AuthErrorNone());
    ASSERT_TRUE(global_error_->HasMenuItem());
    provider1.SetAuthError(GoogleServiceAuthError::AuthErrorNone());
    ASSERT_FALSE(global_error_->HasMenuItem());
  }
  ASSERT_FALSE(global_error_->HasMenuItem());
}

// Verify that SigninGlobalError ignores certain errors.
TEST_F(SigninGlobalErrorTest, AuthStatusEnumerateAllErrors) {
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
  };
  COMPILE_ASSERT(ARRAYSIZE_UNSAFE(table) == GoogleServiceAuthError::NUM_STATES,
      kTable_size_does_not_match_number_of_auth_error_types);

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(table); ++i) {
    FakeAuthStatusProvider provider(global_error_);
    provider.SetAuthError(GoogleServiceAuthError(table[i].error_state));
    EXPECT_EQ(global_error_->HasMenuItem(), table[i].is_error);
    // Only on chromeos do we have a separate menu item - on other platforms
    // there's code in WrenchMenuModel to re-use the "sign in to chrome"
    // menu item to display auth status/errors.
    EXPECT_EQ(global_error_->HasMenuItem(), table[i].is_error);
    EXPECT_EQ(global_error_->MenuItemLabel().empty(), !table[i].is_error);
    EXPECT_EQ(global_error_->GetBubbleViewMessage().empty(),
              !table[i].is_error);
    EXPECT_FALSE(global_error_->GetBubbleViewTitle().empty());
    EXPECT_FALSE(global_error_->GetBubbleViewAcceptButtonLabel().empty());
    EXPECT_TRUE(global_error_->GetBubbleViewCancelButtonLabel().empty());
  }
}
