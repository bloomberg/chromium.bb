// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/signin/signin_browser_state_info_updater.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

#include <memory>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/strings/utf_string_conversions.h"
#include "components/signin/core/browser/account_tracker_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/test_signin_client.h"
#include "components/signin/ios/browser/profile_oauth2_token_service_ios_delegate.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "ios/chrome/browser/application_context.h"
#include "ios/chrome/browser/browser_state/browser_state_info_cache.h"
#include "ios/chrome/browser/browser_state/chrome_browser_state_manager.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state.h"
#include "ios/chrome/browser/browser_state/test_chrome_browser_state_manager.h"
#include "ios/chrome/browser/signin/account_tracker_service_factory.h"
#include "ios/chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "ios/chrome/browser/signin/signin_client_factory.h"
#include "ios/chrome/browser/signin/signin_manager_factory.h"
#include "ios/chrome/test/ios_chrome_scoped_testing_chrome_browser_state_manager.h"
#include "ios/web/public/test/test_web_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace {
// Returns a TestSigninClient.
std::unique_ptr<KeyedService> BuildTestSigninClient(web::BrowserState* state) {
  return std::make_unique<TestSigninClient>(
      ios::ChromeBrowserState::FromBrowserState(state)->GetPrefs());
}

// Builds a fake token service.
std::unique_ptr<KeyedService> BuildTestTokenService(web::BrowserState* state) {
  ios::ChromeBrowserState* browser_state =
      ios::ChromeBrowserState::FromBrowserState(state);
  return std::make_unique<FakeProfileOAuth2TokenService>(
      browser_state->GetPrefs());
}
}  // namespace

class SigninBrowserStateInfoUpdaterTest : public PlatformTest {
 public:
  SigninBrowserStateInfoUpdaterTest() {
    EXPECT_TRUE(temp_directory_.CreateUniqueTempDir());
    // Setup the browser state manager.
    scoped_browser_state_manager_ =
        std::make_unique<IOSChromeScopedTestingChromeBrowserStateManager>(
            std::make_unique<TestChromeBrowserStateManager>(
                temp_directory_.GetPath().DirName()));
    browser_state_info_ = GetApplicationContext()
                              ->GetChromeBrowserStateManager()
                              ->GetBrowserStateInfoCache();
    browser_state_info_->AddBrowserState(temp_directory_.GetPath(),
                                         /*gaia_id=*/std::string(),
                                         /*user_name=*/base::string16());
    // Create the browser state.
    TestChromeBrowserState::Builder test_cbs_builder;
    test_cbs_builder.SetPath(temp_directory_.GetPath());
    test_cbs_builder.AddTestingFactory(
        SigninClientFactory::GetInstance(),
        base::BindRepeating(&BuildTestSigninClient));
    test_cbs_builder.AddTestingFactory(
        ProfileOAuth2TokenServiceFactory::GetInstance(),
        base::BindRepeating(&BuildTestTokenService));
    chrome_browser_state_ = test_cbs_builder.Build();

    cache_index_ = browser_state_info_->GetIndexOfBrowserStateWithPath(
        chrome_browser_state_->GetStatePath());
  }

  web::TestWebThreadBundle thread_bundle_;
  base::ScopedTempDir temp_directory_;
  std::unique_ptr<IOSChromeScopedTestingChromeBrowserStateManager>
      scoped_browser_state_manager_;
  size_t cache_index_ = 0u;
  std::unique_ptr<TestChromeBrowserState> chrome_browser_state_;

  // Weak, owned by scoped_browser_state_manager_.
  BrowserStateInfoCache* browser_state_info_ = nullptr;
};

// Tests that the browser state info is updated on signin and signout.
TEST_F(SigninBrowserStateInfoUpdaterTest, SigninSignout) {
  ASSERT_FALSE(
      browser_state_info_->BrowserStateIsAuthenticatedAtIndex(cache_index_));

  // Signin.
  AccountTrackerService* account_tracker =
      ios::AccountTrackerServiceFactory::GetForBrowserState(
          chrome_browser_state_.get());
  SigninManager* signin_manager = ios::SigninManagerFactory::GetForBrowserState(
      chrome_browser_state_.get());
  std::string account_id =
      account_tracker->SeedAccountInfo("gaia", "example@email.com");
  signin_manager->OnExternalSigninCompleted("example@email.com");
  EXPECT_TRUE(
      browser_state_info_->BrowserStateIsAuthenticatedAtIndex(cache_index_));
  EXPECT_EQ("gaia",
            browser_state_info_->GetGAIAIdOfBrowserStateAtIndex(cache_index_));
  EXPECT_EQ(
      "example@email.com",
      base::UTF16ToUTF8(
          browser_state_info_->GetUserNameOfBrowserStateAtIndex(cache_index_)));

  // Signout.
  signin_manager->SignOut(signin_metrics::SIGNOUT_TEST,
                          signin_metrics::SignoutDelete::IGNORE_METRIC);
  EXPECT_FALSE(
      browser_state_info_->BrowserStateIsAuthenticatedAtIndex(cache_index_));
}

// Tests that the browser state info is updated on auth error change.
TEST_F(SigninBrowserStateInfoUpdaterTest, AuthError) {
  ASSERT_FALSE(
      browser_state_info_->BrowserStateIsAuthenticatedAtIndex(cache_index_));

  // Signin.
  AccountTrackerService* account_tracker =
      ios::AccountTrackerServiceFactory::GetForBrowserState(
          chrome_browser_state_.get());
  SigninManager* signin_manager = ios::SigninManagerFactory::GetForBrowserState(
      chrome_browser_state_.get());
  FakeProfileOAuth2TokenService* token_service =
      static_cast<FakeProfileOAuth2TokenService*>(
          ProfileOAuth2TokenServiceFactory::GetForBrowserState(
              chrome_browser_state_.get()));
  std::string account_id =
      account_tracker->SeedAccountInfo("gaia", "example@email.com");
  token_service->UpdateCredentials(account_id, "token");
  signin_manager->OnExternalSigninCompleted("example@email.com");

  EXPECT_TRUE(
      browser_state_info_->BrowserStateIsAuthenticatedAtIndex(cache_index_));
  EXPECT_FALSE(
      browser_state_info_->BrowserStateIsAuthErrorAtIndex(cache_index_));

  // Set auth error.
  token_service->UpdateAuthErrorForTesting(
      account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
  EXPECT_TRUE(
      browser_state_info_->BrowserStateIsAuthErrorAtIndex(cache_index_));

  // Remove auth error.
  token_service->UpdateAuthErrorForTesting(
      account_id, GoogleServiceAuthError::AuthErrorNone());
  EXPECT_FALSE(
      browser_state_info_->BrowserStateIsAuthErrorAtIndex(cache_index_));
}
