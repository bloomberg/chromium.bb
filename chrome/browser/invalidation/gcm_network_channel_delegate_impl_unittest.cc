// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "chrome/browser/invalidation/gcm_network_channel_delegate_impl.h"
#include "chrome/browser/services/gcm/fake_gcm_profile_service.h"
#include "chrome/browser/services/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service_wrapper.h"
#include "chrome/browser/signin/profile_oauth2_token_service.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace invalidation {
namespace {

class GCMNetworkChannelDelegateImplTest : public ::testing::Test {
 protected:
  GCMNetworkChannelDelegateImplTest() {}

  virtual ~GCMNetworkChannelDelegateImplTest() {}

  virtual void SetUp() OVERRIDE {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(
        ProfileOAuth2TokenServiceFactory::GetInstance(),
        FakeProfileOAuth2TokenServiceWrapper::BuildAutoIssuingTokenService);
    profile_ = builder.Build();

    FakeProfileOAuth2TokenService* token_service =
        (FakeProfileOAuth2TokenService*)
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile_.get());
    token_service->IssueRefreshTokenForUser("", "refresh_token");

    delegate_.reset(new GCMNetworkChannelDelegateImpl(profile_.get()));
  }

 public:
  void RegisterFinished(const std::string& registration_id,
                        gcm::GCMClient::Result result) {}

  void RequestTokenFinished(const GoogleServiceAuthError& error,
                            const std::string& token) {
    issued_tokens_.push_back(token);
    request_token_errors_.push_back(error);
  }

  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<Profile> profile_;
  FakeProfileOAuth2TokenService* token_service_;

  std::vector<std::string> issued_tokens_;
  std::vector<GoogleServiceAuthError> request_token_errors_;

  scoped_ptr<GCMNetworkChannelDelegateImpl> delegate_;
};

TEST_F(GCMNetworkChannelDelegateImplTest, RequestToken) {
  // Make sure that call to RequestToken reaches OAuth2TokenService and gets
  // back to callback.
  delegate_->RequestToken(
      base::Bind(&GCMNetworkChannelDelegateImplTest::RequestTokenFinished,
                 base::Unretained(this)));
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
  EXPECT_EQ(1U, issued_tokens_.size());
  EXPECT_NE("", issued_tokens_[0]);
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(), request_token_errors_[0]);
}

TEST_F(GCMNetworkChannelDelegateImplTest, RequestTokenTwoConcurrentRequests) {
  // First call should finish with REQUEST_CANCELLED error.
  delegate_->RequestToken(
      base::Bind(&GCMNetworkChannelDelegateImplTest::RequestTokenFinished,
                 base::Unretained(this)));
  // Second request should succeed.
  delegate_->RequestToken(
      base::Bind(&GCMNetworkChannelDelegateImplTest::RequestTokenFinished,
                 base::Unretained(this)));
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  EXPECT_EQ(2U, issued_tokens_.size());

  EXPECT_EQ("", issued_tokens_[0]);
  EXPECT_EQ(GoogleServiceAuthError::REQUEST_CANCELED,
            request_token_errors_[0].state());

  EXPECT_NE("", issued_tokens_[1]);
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(), request_token_errors_[1]);
}

}  // namespace
}  // namespace invalidation
