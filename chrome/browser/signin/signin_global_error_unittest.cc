// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_global_error.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/signin_manager_fake.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

class FakeAuthStatusProvider : public SigninGlobalError::AuthStatusProvider {
 public:
  FakeAuthStatusProvider() : auth_error_(GoogleServiceAuthError::None()) {}

  // AuthStatusProvider implementation.
  GoogleServiceAuthError GetAuthStatus() const OVERRIDE { return auth_error_; }

  void set_auth_error(const GoogleServiceAuthError& error) {
    auth_error_ = error;
  }

 private:
  GoogleServiceAuthError auth_error_;
};

class SigninGlobalErrorTest : public testing::Test {
 public:
  void SetUp() OVERRIDE {
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
  ASSERT_FALSE(global_error_->HasBadge());
}

TEST_F(SigninGlobalErrorTest, NoErrorAuthStatusProviders) {
  FakeAuthStatusProvider provider;
  global_error_->AddProvider(&provider);
  ASSERT_FALSE(global_error_->HasBadge());
  global_error_->RemoveProvider(&provider);
  ASSERT_FALSE(global_error_->HasBadge());
}

TEST_F(SigninGlobalErrorTest, ErrorAuthStatusProvider) {
  FakeAuthStatusProvider provider;
  FakeAuthStatusProvider error_provider;
  error_provider.set_auth_error(GoogleServiceAuthError(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  global_error_->AddProvider(&provider);
  ASSERT_FALSE(global_error_->HasBadge());
  global_error_->AddProvider(&error_provider);
  ASSERT_TRUE(global_error_->HasBadge());
  global_error_->RemoveProvider(&error_provider);
  ASSERT_FALSE(global_error_->HasBadge());
  global_error_->RemoveProvider(&provider);
  ASSERT_FALSE(global_error_->HasBadge());
}

TEST_F(SigninGlobalErrorTest, AuthStatusProviderErrorTransition) {
  FakeAuthStatusProvider provider0;
  FakeAuthStatusProvider provider1;
  global_error_->AddProvider(&provider0);
  global_error_->AddProvider(&provider1);
  ASSERT_FALSE(global_error_->HasBadge());
  provider0.set_auth_error(
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  ASSERT_FALSE(global_error_->HasBadge());
  global_error_->AuthStatusChanged();
  ASSERT_TRUE(global_error_->HasBadge());
  provider1.set_auth_error(
      GoogleServiceAuthError(GoogleServiceAuthError::ACCOUNT_DISABLED));
  global_error_->AuthStatusChanged();
  ASSERT_TRUE(global_error_->HasBadge());

  // Now resolve the auth errors - badge should go away.
  provider0.set_auth_error(GoogleServiceAuthError::None());
  global_error_->AuthStatusChanged();
  ASSERT_TRUE(global_error_->HasBadge());
  provider1.set_auth_error(GoogleServiceAuthError::None());
  global_error_->AuthStatusChanged();
  ASSERT_FALSE(global_error_->HasBadge());

  global_error_->RemoveProvider(&provider0);
  ASSERT_FALSE(global_error_->HasBadge());
  global_error_->RemoveProvider(&provider1);
  ASSERT_FALSE(global_error_->HasBadge());
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
    FakeAuthStatusProvider provider;
    provider.set_auth_error(GoogleServiceAuthError(table[i].error_state));
    GlobalErrorService* service =
        GlobalErrorServiceFactory::GetForProfile(profile_.get());
    global_error_->AddProvider(&provider);
    EXPECT_EQ(global_error_->HasBadge(), table[i].is_error);
    // Should badge the wrench menu if there's an error.
    EXPECT_EQ(service->GetFirstBadgeResourceID() != 0, table[i].is_error);
#if defined(OS_CHROMEOS)
    // Only on chromeos do we have a separate menu item - on other platforms
    // there's code in WrenchMenuModel to re-use the "sign in to chrome"
    // menu item to display auth status/errors.
    EXPECT_EQ(global_error_->HasMenuItem(), table[i].is_error);
#else
    EXPECT_FALSE(global_error_->HasMenuItem());
#endif
    EXPECT_EQ(global_error_->MenuItemLabel().empty(), !table[i].is_error);
    EXPECT_EQ(global_error_->GetBubbleViewMessage().empty(),
              !table[i].is_error);
    EXPECT_FALSE(global_error_->GetBubbleViewTitle().empty());
    EXPECT_FALSE(global_error_->GetBubbleViewAcceptButtonLabel().empty());
    EXPECT_TRUE(global_error_->GetBubbleViewCancelButtonLabel().empty());
    global_error_->RemoveProvider(&provider);
  }
}
