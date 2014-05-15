// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_global_error.h"

#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service_builder.h"
#include "chrome/browser/signin/fake_signin_manager.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_global_error_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/core/browser/fake_auth_status_provider.h"
#include "components/signin/core/browser/signin_error_controller.h"
#include "components/signin/core/browser/signin_manager.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

static const char kTestAccountId[] = "testuser@test.com";
static const char kTestUsername[] = "testuser@test.com";

class SigninGlobalErrorTest : public testing::Test {
 public:
  virtual void SetUp() OVERRIDE {
    // Create a signed-in profile.
    TestingProfile::Builder builder;
    builder.AddTestingFactory(ProfileOAuth2TokenServiceFactory::GetInstance(),
                              BuildFakeProfileOAuth2TokenService);
    builder.AddTestingFactory(SigninManagerFactory::GetInstance(),
                              FakeSigninManagerBase::Build);
    profile_ = builder.Build();

    profile_->GetPrefs()->SetString(
        prefs::kGoogleServicesUsername, kTestAccountId);
    SigninManagerFactory::GetForProfile(profile_.get())
        ->SetAuthenticatedUsername(kTestAccountId);

    global_error_ = SigninGlobalErrorFactory::GetForProfile(profile_.get());
    error_controller_ = ProfileOAuth2TokenServiceFactory::GetForProfile(
        profile_.get())->signin_error_controller();
  }

  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<TestingProfile> profile_;
  SigninGlobalError* global_error_;
  SigninErrorController* error_controller_;
};

TEST_F(SigninGlobalErrorTest, NoErrorAuthStatusProviders) {
  scoped_ptr<FakeAuthStatusProvider> provider;

  ASSERT_FALSE(global_error_->HasMenuItem());

  // Add a provider.
  provider.reset(new FakeAuthStatusProvider(error_controller_));
  ASSERT_FALSE(global_error_->HasMenuItem());

  // Remove the provider.
  provider.reset();
  ASSERT_FALSE(global_error_->HasMenuItem());
}

TEST_F(SigninGlobalErrorTest, ErrorAuthStatusProvider) {
  scoped_ptr<FakeAuthStatusProvider> provider;
  scoped_ptr<FakeAuthStatusProvider> error_provider;

  provider.reset(new FakeAuthStatusProvider(error_controller_));
  ASSERT_FALSE(global_error_->HasMenuItem());

  error_provider.reset(new FakeAuthStatusProvider(error_controller_));
  error_provider->SetAuthError(
      kTestAccountId,
      kTestUsername,
      GoogleServiceAuthError(
          GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  ASSERT_TRUE(global_error_->HasMenuItem());

  error_provider.reset();
  ASSERT_FALSE(global_error_->HasMenuItem());

  provider.reset();
  error_provider.reset();
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
    { GoogleServiceAuthError::UNEXPECTED_SERVICE_RESPONSE, true },
    { GoogleServiceAuthError::SERVICE_ERROR, true },
  };
  COMPILE_ASSERT(ARRAYSIZE_UNSAFE(table) == GoogleServiceAuthError::NUM_STATES,
      kTable_size_does_not_match_number_of_auth_error_types);

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(table); ++i) {
    FakeAuthStatusProvider provider(error_controller_);
    provider.SetAuthError(kTestAccountId,
                          kTestUsername,
                          GoogleServiceAuthError(table[i].error_state));

    EXPECT_EQ(global_error_->HasMenuItem(), table[i].is_error);
    EXPECT_EQ(global_error_->MenuItemLabel().empty(), !table[i].is_error);
    EXPECT_EQ(global_error_->GetBubbleViewMessages().empty(),
              !table[i].is_error);
    EXPECT_FALSE(global_error_->GetBubbleViewTitle().empty());
    EXPECT_FALSE(global_error_->GetBubbleViewAcceptButtonLabel().empty());
    EXPECT_TRUE(global_error_->GetBubbleViewCancelButtonLabel().empty());
  }
}
