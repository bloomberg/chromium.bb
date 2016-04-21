// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "components/signin/core/browser/fake_account_fetcher_service.h"
#include "components/signin/core/browser/fake_profile_oauth2_token_service.h"
#include "components/signin/core/browser/profile_oauth2_token_service.h"
#include "components/sync_driver/profile_sync_auth_provider.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gtest/include/gtest/gtest.h"

const char kAccountId[] = "account.id";

class ProfileSyncAuthProviderTest : public ::testing::Test {
 public:
  ProfileSyncAuthProviderTest() {}

  ~ProfileSyncAuthProviderTest() override {}

  void SetUp() override {
    token_service_.reset(new FakeProfileOAuth2TokenService());
    token_service_->set_auto_post_fetch_response_on_message_loop(true);
    token_service_->UpdateCredentials(kAccountId, "fake_refresh_token");

    auth_provider_frontend_.reset(new ProfileSyncAuthProvider(
        token_service_.get(), kAccountId,
        GaiaConstants::kChromeSyncOAuth2Scope));
    auth_provider_backend_ =
        auth_provider_frontend_->CreateProviderForSyncThread();
  }

  void RequestTokenFinished(const GoogleServiceAuthError& error,
                            const std::string& token) {
    issued_tokens_.push_back(token);
    request_token_errors_.push_back(error);
  }

 protected:
  base::MessageLoop message_loop_;

  std::unique_ptr<FakeProfileOAuth2TokenService> token_service_;
  std::unique_ptr<ProfileSyncAuthProvider> auth_provider_frontend_;
  std::unique_ptr<syncer::SyncAuthProvider> auth_provider_backend_;

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
