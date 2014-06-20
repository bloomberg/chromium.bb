// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/run_loop.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service.h"
#include "chrome/browser/signin/fake_profile_oauth2_token_service_builder.h"
#include "chrome/browser/signin/profile_oauth2_token_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/gcm_driver/fake_gcm_driver.h"
#include "components/gcm_driver/gcm_driver.h"
#include "components/invalidation/gcm_invalidation_bridge.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/gaia/fake_identity_provider.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "net/base/ip_endpoint.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace invalidation {
namespace {

// Implementation of GCMDriver::Register that always succeeds with the same
// registrationId.
class CustomFakeGCMDriver : public gcm::FakeGCMDriver {
 public:
  CustomFakeGCMDriver() {}
  virtual ~CustomFakeGCMDriver() {}

 protected:
  // FakeGCMDriver override:
  virtual void RegisterImpl(
      const std::string& app_id,
      const std::vector<std::string>& sender_ids) OVERRIDE {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&CustomFakeGCMDriver::RegisterFinished,
                   base::Unretained(this),
                   app_id,
                   std::string("registration.id"),
                   gcm::GCMClient::SUCCESS));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(CustomFakeGCMDriver);
};

class GCMInvalidationBridgeTest : public ::testing::Test {
 protected:
  GCMInvalidationBridgeTest()
      : connection_online_(false) {}

  virtual ~GCMInvalidationBridgeTest() {}

  virtual void SetUp() OVERRIDE {
    TestingProfile::Builder builder;
    builder.AddTestingFactory(ProfileOAuth2TokenServiceFactory::GetInstance(),
                              &BuildAutoIssuingFakeProfileOAuth2TokenService);
    profile_ = builder.Build();

    FakeProfileOAuth2TokenService* token_service =
        (FakeProfileOAuth2TokenService*)
        ProfileOAuth2TokenServiceFactory::GetForProfile(profile_.get());
    token_service->IssueRefreshTokenForUser("", "fake_refresh_token");
    gcm_driver_.reset(new CustomFakeGCMDriver());

    identity_provider_.reset(new FakeIdentityProvider(token_service));
    bridge_.reset(new GCMInvalidationBridge(gcm_driver_.get(),
                                            identity_provider_.get()));

    delegate_ = bridge_->CreateDelegate();
    delegate_->Initialize(
        base::Bind(&GCMInvalidationBridgeTest::ConnectionStateChanged,
                   base::Unretained(this)));
    RunLoop();
  }

  void RunLoop() {
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

  void ConnectionStateChanged(bool online) {
    connection_online_ = online;
  }

  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<Profile> profile_;
  scoped_ptr<gcm::GCMDriver> gcm_driver_;
  scoped_ptr<FakeIdentityProvider> identity_provider_;

  std::vector<std::string> issued_tokens_;
  std::vector<GoogleServiceAuthError> request_token_errors_;
  std::string registration_id_;
  bool connection_online_;

  scoped_ptr<GCMInvalidationBridge> bridge_;
  scoped_ptr<syncer::GCMNetworkChannelDelegate> delegate_;
};

TEST_F(GCMInvalidationBridgeTest, RequestToken) {
  // Make sure that call to RequestToken reaches OAuth2TokenService and gets
  // back to callback.
  delegate_->RequestToken(
      base::Bind(&GCMInvalidationBridgeTest::RequestTokenFinished,
                 base::Unretained(this)));
  RunLoop();
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
  RunLoop();

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
  RunLoop();

  EXPECT_FALSE(registration_id_.empty());
}

TEST_F(GCMInvalidationBridgeTest, ConnectionState) {
  EXPECT_FALSE(connection_online_);
  bridge_->OnConnected(net::IPEndPoint());
  RunLoop();
  EXPECT_TRUE(connection_online_);
  bridge_->OnDisconnected();
  RunLoop();
  EXPECT_FALSE(connection_online_);
}

}  // namespace
}  // namespace invalidation
