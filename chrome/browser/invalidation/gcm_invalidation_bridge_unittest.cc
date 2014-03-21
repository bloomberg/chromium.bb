// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "chrome/browser/invalidation/gcm_invalidation_bridge.h"
#include "chrome/browser/invalidation/invalidation_auth_provider.h"
#include "chrome/browser/services/gcm/gcm_profile_service.h"
#include "chrome/browser/services/gcm/gcm_profile_service_factory.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service_builder.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/signin/core/profile_oauth2_token_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace invalidation {
namespace {

// Implementation of GCMProfileService::Register that always succeeds with the
// same registrationId.
class FakeGCMProfileService : public gcm::GCMProfileService {
 public:
  static KeyedService* Build(content::BrowserContext* context) {
    Profile* profile = static_cast<Profile*>(context);
    return new FakeGCMProfileService(profile);
  }

  explicit FakeGCMProfileService(Profile* profile)
      : gcm::GCMProfileService(profile) {}

  virtual void Register(const std::string& app_id,
                        const std::vector<std::string>& sender_ids,
                        RegisterCallback callback) OVERRIDE {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(
            callback, std::string("registration.id"), gcm::GCMClient::SUCCESS));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(FakeGCMProfileService);
};

// Fake invalidation auth provider implementation.
class FakeInvalidationAuthProvider : public InvalidationAuthProvider {
 public:
  explicit FakeInvalidationAuthProvider(
      ProfileOAuth2TokenService* token_service)
      : token_service_(token_service) {}
  virtual ~FakeInvalidationAuthProvider() {}

  // InvalidationAuthProvider:
  virtual OAuth2TokenService* GetTokenService() OVERRIDE {
    return token_service_;
  }
  virtual std::string GetAccountId() OVERRIDE { return std::string(); }
  virtual bool ShowLoginUI() OVERRIDE { return false; }

 private:
  OAuth2TokenService* token_service_;

  DISALLOW_COPY_AND_ASSIGN(FakeInvalidationAuthProvider);
};

class GCMInvalidationBridgeTest : public ::testing::Test {
 protected:
  GCMInvalidationBridgeTest() {}

  virtual ~GCMInvalidationBridgeTest() {}

  virtual void SetUp() OVERRIDE {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(ProfileOAuth2TokenServiceFactory::GetInstance(),
                              &BuildAutoIssuingFakeProfileOAuth2TokenService);
    builder.AddTestingFactory(gcm::GCMProfileServiceFactory::GetInstance(),
                              &FakeGCMProfileService::Build);
    profile_ = builder.Build();

    FakeProfileOAuth2TokenService* token_service =
        (FakeProfileOAuth2TokenService*)
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile_.get());
    token_service->IssueRefreshTokenForUser("", "fake_refresh_token");
    gcm_profile_service_ =
        (FakeGCMProfileService*)gcm::GCMProfileServiceFactory::GetForProfile(
            profile_.get());

    auth_provider_.reset(new FakeInvalidationAuthProvider(token_service));
    bridge_.reset(
        new GCMInvalidationBridge(gcm_profile_service_, auth_provider_.get()));

    delegate_ = bridge_->CreateDelegate();
    delegate_->Initialize();
    base::RunLoop run_loop;
    run_loop.RunUntilIdle();
  }

 public:
  void RegisterFinished(const std::string& registration_id,
                        gcm::GCMClient::Result result) {
    registration_id_ = registration_id;
  }

  void RequestTokenFinished(const GoogleServiceAuthError& error,
                            const std::string& token) {
    issued_tokens_.push_back(token);
    request_token_errors_.push_back(error);
  }

  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<Profile> profile_;
  FakeGCMProfileService* gcm_profile_service_;
  scoped_ptr<FakeInvalidationAuthProvider> auth_provider_;

  std::vector<std::string> issued_tokens_;
  std::vector<GoogleServiceAuthError> request_token_errors_;
  std::string registration_id_;

  scoped_ptr<GCMInvalidationBridge> bridge_;
  scoped_ptr<syncer::GCMNetworkChannelDelegate> delegate_;
};

TEST_F(GCMInvalidationBridgeTest, RequestToken) {
  // Make sure that call to RequestToken reaches OAuth2TokenService and gets
  // back to callback.
  delegate_->RequestToken(
      base::Bind(&GCMInvalidationBridgeTest::RequestTokenFinished,
                 base::Unretained(this)));
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();
  EXPECT_EQ(1U, issued_tokens_.size());
  EXPECT_NE("", issued_tokens_[0]);
  EXPECT_EQ(GoogleServiceAuthError::AuthErrorNone(), request_token_errors_[0]);
}

TEST_F(GCMInvalidationBridgeTest, RequestTokenTwoConcurrentRequests) {
  // First call should finish with REQUEST_CANCELLED error.
  delegate_->RequestToken(
      base::Bind(&GCMInvalidationBridgeTest::RequestTokenFinished,
                 base::Unretained(this)));
  // Second request should succeed.
  delegate_->RequestToken(
      base::Bind(&GCMInvalidationBridgeTest::RequestTokenFinished,
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

TEST_F(GCMInvalidationBridgeTest, Register) {
  EXPECT_TRUE(registration_id_.empty());
  delegate_->Register(base::Bind(&GCMInvalidationBridgeTest::RegisterFinished,
                                 base::Unretained(this)));
  base::RunLoop run_loop;
  run_loop.RunUntilIdle();

  EXPECT_FALSE(registration_id_.empty());
}

}  // namespace
}  // namespace invalidation
