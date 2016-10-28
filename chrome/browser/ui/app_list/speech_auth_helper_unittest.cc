// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/app_list/speech_auth_helper.h"

#include <memory>
#include <utility>

#include "base/strings/utf_string_conversions.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service_builder.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/signin_manager.h"
#include "components/signin/core/browser/signin_manager_base.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace app_list {

static const char* kTestGaiaId = "gaia_id";
static const char* kTestUser = "test.user@chromium.org.test";
static const char* kScope = "https://www.googleapis.com/auth/webhistory";
static const char* kAccessToken = "fake_access_token";

class SpeechAuthHelperTest : public testing::Test {
 public:
  SpeechAuthHelperTest()
      : testing_profile_manager_(new TestingProfileManager(
          TestingBrowserProcess::GetGlobal())) {
  }

  void SetUp() override {
    // Set up FakeProfileOAuth2TokenService.
    TestingProfile::TestingFactories factories;
    factories.push_back(std::make_pair(
        ProfileOAuth2TokenServiceFactory::GetInstance(),
        &BuildAutoIssuingFakeProfileOAuth2TokenService));

    ASSERT_TRUE(testing_profile_manager_->SetUp());
    profile_ = testing_profile_manager_->CreateTestingProfile(
        kTestUser, std::unique_ptr<sync_preferences::PrefServiceSyncable>(),
        base::UTF8ToUTF16(kTestUser), 0, std::string(), factories);

    // Set up the authenticated user name and ID.
    SigninManagerFactory::GetForProfile(profile_)->SetAuthenticatedAccountInfo(
        kTestGaiaId, kTestUser);
  }

 protected:
  void SetupRefreshToken() {
    std::string account_id = SigninManagerFactory::GetForProfile(profile_)
                                 ->GetAuthenticatedAccountId();
    GetFakeProfileOAuth2TokenService()->UpdateCredentials(account_id,
                                                          "fake_refresh_token");
  }

  FakeProfileOAuth2TokenService* GetFakeProfileOAuth2TokenService() {
    return static_cast<FakeProfileOAuth2TokenService*>(
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile_));
  }

  base::SimpleTestClock test_clock_;
  content::TestBrowserThreadBundle thread_bundle;
  std::unique_ptr<TestingProfileManager> testing_profile_manager_;
  Profile* profile_;
  std::unique_ptr<SpeechAuthHelper> auth_helper_;
};

TEST_F(SpeechAuthHelperTest, TokenFetch) {
  SetupRefreshToken();
  SpeechAuthHelper helper(profile_, &test_clock_);
  EXPECT_TRUE(helper.GetToken().empty());

  OAuth2TokenService::ScopeSet scopes;
  scopes.insert(kScope);
  GetFakeProfileOAuth2TokenService()->IssueTokenForScope(scopes,
                                                         kAccessToken,
                                                         base::Time::Max());

  EXPECT_EQ(kAccessToken, helper.GetToken());
  EXPECT_EQ(kScope, helper.GetScope());
}

TEST_F(SpeechAuthHelperTest, TokenFetchDelayedRefreshToken) {
  SpeechAuthHelper helper(profile_, &test_clock_);
  SetupRefreshToken();
  EXPECT_TRUE(helper.GetToken().empty());

  OAuth2TokenService::ScopeSet scopes;
  scopes.insert(kScope);
  GetFakeProfileOAuth2TokenService()->IssueTokenForScope(scopes,
                                                         kAccessToken,
                                                         base::Time::Max());

  EXPECT_EQ(kAccessToken, helper.GetToken());
  EXPECT_EQ(kScope, helper.GetScope());
}

TEST_F(SpeechAuthHelperTest, TokenFetchFailed) {
  SetupRefreshToken();
  SpeechAuthHelper helper(profile_, &test_clock_);
  EXPECT_TRUE(helper.GetToken().empty());

  OAuth2TokenService::ScopeSet scopes;
  scopes.insert(kScope);
  GetFakeProfileOAuth2TokenService()->IssueErrorForScope(
      scopes, GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_ERROR));

  EXPECT_TRUE(helper.GetToken().empty());
  EXPECT_EQ(kScope, helper.GetScope());
}

}  // namespace app_list
