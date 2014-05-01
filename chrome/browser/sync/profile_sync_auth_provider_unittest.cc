// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service_builder.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/browser/sync/profile_sync_auth_provider.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gtest/include/gtest/gtest.h"

const char kAccountId[] = "account.id";

class ProfileSyncAuthProviderTest : public ::testing::Test {
 public:
  ProfileSyncAuthProviderTest() {}

  virtual ~ProfileSyncAuthProviderTest() {}

  virtual void SetUp() OVERRIDE {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(ProfileOAuth2TokenServiceFactory::GetInstance(),
                              &BuildAutoIssuingFakeProfileOAuth2TokenService);

    profile_ = builder.Build();

    FakeProfileOAuth2TokenService* token_service =
        (FakeProfileOAuth2TokenService*)
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile_.get());
    token_service->IssueRefreshTokenForUser(kAccountId, "fake_refresh_token");

    auth_provider_frontend_.reset(new ProfileSyncAuthProvider(
        token_service, kAccountId, GaiaConstants::kChromeSyncOAuth2Scope));
    auth_provider_backend_ =
        auth_provider_frontend_->CreateProviderForSyncThread().Pass();
  }

  void RequestTokenFinished(const GoogleServiceAuthError& error,
                            const std::string& token) {
    issued_tokens_.push_back(token);
    request_token_errors_.push_back(error);
  }

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<Profile> profile_;

  scoped_ptr<ProfileSyncAuthProvider> auth_provider_frontend_;
  scoped_ptr<syncer::SyncAuthProvider> auth_provider_backend_;

  std::vector<std::string> issued_tokens_;
  std::vector<GoogleServiceAuthError> request_token_errors_;
};

TEST_F(ProfileSyncAuthProviderTest, RequestToken) {
  // Request access token, make sure it gets valid response.
  auth_provider_backend_->RequestAccessToken(
      base::Bind(&ProfileSyncAuthProviderTest::RequestTokenFinished,
                 base::Unretained(this)));
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
  EXPECT_EQ(1U, request_token_errors_.size());
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(), request_token_errors_[0]);
  EXPECT_NE("", issued_tokens_[0]);
}

TEST_F(ProfileSyncAuthProviderTest, RequestTokenTwoConcurrentRequests) {
  // Start two requests for access token. One should be reported as canceled,
  // the other one should succeed.
  auth_provider_backend_->RequestAccessToken(
      base::Bind(&ProfileSyncAuthProviderTest::RequestTokenFinished,
                 base::Unretained(this)));
  auth_provider_backend_->RequestAccessToken(
      base::Bind(&ProfileSyncAuthProviderTest::RequestTokenFinished,
                 base::Unretained(this)));
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
  EXPECT_EQ(2U, request_token_errors_.size());

  EXPECT_EQ("", issued_tokens_[0]);
  EXPECT_EQ(GoogleServiceAuthError::REQUEST_CANCELED,
            request_token_errors_[0].state());

  EXPECT_NE("", issued_tokens_[1]);
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(), request_token_errors_[1]);
}
