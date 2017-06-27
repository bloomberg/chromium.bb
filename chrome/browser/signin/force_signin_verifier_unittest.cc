// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/force_signin_verifier.h"

#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

class MockForceSigninVerifier : public ForceSigninVerifier {
 public:
  explicit MockForceSigninVerifier(Profile* profile)
      : ForceSigninVerifier(profile) {}

  bool ShouldSendRequest() override { return true; }

  void SendTestRequest() { SendRequest(); }

  bool IsDelayTaskPosted() { return GetOneShotTimerForTesting()->IsRunning(); }

  int FailureCount() { return GetBackoffEntryForTesting()->failure_count(); }

  OAuth2TokenService::Request* request() { return GetRequestForTesting(); }

  bool IsCountdownTimerRunning() {
    base::Timer* timer = GetWindowCloseTimerForTesting();
    return timer && timer->IsRunning();
  }

  void OnShowDialog() { StartCountdown(); }

  MOCK_METHOD0(ShowDialog, void(void));
};

class ForceSigninVerifierTest : public ::testing::Test {
 public:
  void SetUp() override {
    verifier_ = base::MakeUnique<MockForceSigninVerifier>(&profile_);
  }

  void TearDown() override { verifier_.reset(); }

  std::unique_ptr<MockForceSigninVerifier> verifier_;
  content::TestBrowserThreadBundle bundle_;
  TestingProfile profile_;

  GoogleServiceAuthError persistent_error_ = GoogleServiceAuthError(
      GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS);
  GoogleServiceAuthError transient_error_ =
      GoogleServiceAuthError(GoogleServiceAuthError::State::CONNECTION_FAILED);
};

TEST_F(ForceSigninVerifierTest, OnGetTokenSuccess) {
  ASSERT_EQ(nullptr, verifier_->request());

  verifier_->SendTestRequest();

  ASSERT_NE(nullptr, verifier_->request());
  ASSERT_FALSE(verifier_->HasTokenBeenVerified());
  ASSERT_FALSE(verifier_->IsDelayTaskPosted());
  EXPECT_CALL(*verifier_.get(), ShowDialog()).Times(0);

  verifier_->OnGetTokenSuccess(verifier_->request(), "", base::Time::Now());
  ASSERT_EQ(nullptr, verifier_->request());
  ASSERT_TRUE(verifier_->HasTokenBeenVerified());
  ASSERT_FALSE(verifier_->IsDelayTaskPosted());
  ASSERT_EQ(0, verifier_->FailureCount());
}

TEST_F(ForceSigninVerifierTest, OnGetTokenPersistentFailure) {
  ASSERT_EQ(nullptr, verifier_->request());

  verifier_->SendTestRequest();

  ASSERT_NE(nullptr, verifier_->request());
  ASSERT_FALSE(verifier_->HasTokenBeenVerified());
  ASSERT_FALSE(verifier_->IsDelayTaskPosted());
  EXPECT_CALL(*verifier_.get(), ShowDialog()).Times(1);

  verifier_->OnGetTokenFailure(verifier_->request(), persistent_error_);
  ASSERT_EQ(nullptr, verifier_->request());
  ASSERT_TRUE(verifier_->HasTokenBeenVerified());
  ASSERT_FALSE(verifier_->IsDelayTaskPosted());
  ASSERT_EQ(0, verifier_->FailureCount());
}

TEST_F(ForceSigninVerifierTest, OnGetTokenTransientFailure) {
  ASSERT_EQ(nullptr, verifier_->request());

  verifier_->SendTestRequest();
  ASSERT_NE(nullptr, verifier_->request());
  ASSERT_FALSE(verifier_->HasTokenBeenVerified());
  ASSERT_FALSE(verifier_->IsDelayTaskPosted());
  EXPECT_CALL(*verifier_.get(), ShowDialog()).Times(0);

  verifier_->OnGetTokenFailure(verifier_->request(), transient_error_);
  ASSERT_EQ(nullptr, verifier_->request());
  ASSERT_FALSE(verifier_->HasTokenBeenVerified());
  ASSERT_TRUE(verifier_->IsDelayTaskPosted());
  ASSERT_EQ(1, verifier_->FailureCount());
}

TEST_F(ForceSigninVerifierTest, OnLostConnection) {
  verifier_->SendTestRequest();
  verifier_->OnGetTokenFailure(verifier_->request(), transient_error_);
  ASSERT_EQ(1, verifier_->FailureCount());
  ASSERT_EQ(nullptr, verifier_->request());
  ASSERT_TRUE(verifier_->IsDelayTaskPosted());

  verifier_->OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType::CONNECTION_NONE);

  ASSERT_EQ(0, verifier_->FailureCount());
  ASSERT_EQ(nullptr, verifier_->request());
  ASSERT_FALSE(verifier_->IsDelayTaskPosted());
}

TEST_F(ForceSigninVerifierTest, OnReconnected) {
  verifier_->SendTestRequest();
  verifier_->OnGetTokenFailure(verifier_->request(), transient_error_);
  ASSERT_EQ(1, verifier_->FailureCount());
  ASSERT_EQ(nullptr, verifier_->request());
  ASSERT_TRUE(verifier_->IsDelayTaskPosted());

  verifier_->OnNetworkChanged(
      net::NetworkChangeNotifier::ConnectionType::CONNECTION_WIFI);

  ASSERT_EQ(0, verifier_->FailureCount());
  ASSERT_NE(nullptr, verifier_->request());
  ASSERT_FALSE(verifier_->IsDelayTaskPosted());
}

TEST_F(ForceSigninVerifierTest, OnGetTokenPersistentFailureAndStartCountdown) {
  ASSERT_EQ(nullptr, verifier_->request());
  ASSERT_FALSE(verifier_->IsCountdownTimerRunning());
  EXPECT_CALL(*verifier_.get(), ShowDialog())
      .WillOnce(::testing::Invoke(verifier_.get(),
                                  &MockForceSigninVerifier::OnShowDialog));

  verifier_->SendTestRequest();
  verifier_->OnGetTokenFailure(verifier_->request(), persistent_error_);

  ASSERT_TRUE(verifier_->IsCountdownTimerRunning());
}
