// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/login/auth/mock_url_fetchers.h"
#include "chrome/browser/chromeos/login/auth/online_attempt.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/login/auth/auth_attempt_state.h"
#include "chromeos/login/auth/mock_auth_attempt_state_resolver.h"
#include "chromeos/login/auth/test_attempt_state.h"
#include "chromeos/login/auth/user_context.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/mock_url_fetcher_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::_;
using content::BrowserThread;

namespace chromeos {

class OnlineAttemptTest : public testing::Test {
 public:
  OnlineAttemptTest()
      : state_(UserContext(), false),
        attempt_(new OnlineAttempt(&state_, &resolver_)) {
  }

  void RunFailureTest(const GoogleServiceAuthError& error) {
    EXPECT_CALL(resolver_, Resolve())
        .Times(1)
        .RetiresOnSaturation();

    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&OnlineAttempt::OnClientLoginFailure,
                   attempt_->weak_factory_.GetWeakPtr(),
                   error));
    // Force UI thread to finish tasks so I can verify |state_|.
    base::RunLoop().RunUntilIdle();
    EXPECT_TRUE(error == state_.online_outcome().error());
  }

  void CancelLogin(OnlineAttempt* auth) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&OnlineAttempt::CancelClientLogin,
                   auth->weak_factory_.GetWeakPtr()));
  }

  content::TestBrowserThreadBundle thread_bundle_;
  TestAttemptState state_;
  MockAuthAttemptStateResolver resolver_;
  scoped_ptr<OnlineAttempt> attempt_;
};

TEST_F(OnlineAttemptTest, LoginSuccess) {
  EXPECT_CALL(resolver_, Resolve())
      .Times(1)
      .RetiresOnSaturation();

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&OnlineAttempt::OnClientLoginSuccess,
                 attempt_->weak_factory_.GetWeakPtr(),
                 GaiaAuthConsumer::ClientLoginResult()));
  // Force UI thread to finish tasks so I can verify |state_|.
  base::RunLoop().RunUntilIdle();
}

TEST_F(OnlineAttemptTest, LoginCancelRetry) {
  GoogleServiceAuthError error(GoogleServiceAuthError::REQUEST_CANCELED);
  TestingProfile profile;

  base::RunLoop run_loop;
  EXPECT_CALL(resolver_, Resolve())
      .WillOnce(Invoke(&run_loop, &base::RunLoop::Quit))
      .RetiresOnSaturation();

  // This is how we inject fake URLFetcher objects, with a factory.
  // This factory creates fake URLFetchers that Start() a fake fetch attempt
  // and then come back on the UI thread saying they've been canceled.
  MockURLFetcherFactory<GotCanceledFetcher> factory;

  attempt_->Initiate(&profile);

  run_loop.Run();

  EXPECT_TRUE(error == state_.online_outcome().error());
  EXPECT_EQ(AuthFailure::NETWORK_AUTH_FAILED, state_.online_outcome().reason());
}

TEST_F(OnlineAttemptTest, LoginTimeout) {
  AuthFailure error(AuthFailure::LOGIN_TIMED_OUT);
  TestingProfile profile;

  base::RunLoop run_loop;
  EXPECT_CALL(resolver_, Resolve())
      .WillOnce(Invoke(&run_loop, &base::RunLoop::Quit))
      .RetiresOnSaturation();

  // This is how we inject fake URLFetcher objects, with a factory.
  // This factory creates fake URLFetchers that Start() a fake fetch attempt
  // and then come back on the UI thread saying they've been canceled.
  MockURLFetcherFactory<ExpectCanceledFetcher> factory;

  attempt_->Initiate(&profile);

  // Post a task to cancel the login attempt.
  CancelLogin(attempt_.get());

  run_loop.Run();

  EXPECT_EQ(AuthFailure::LOGIN_TIMED_OUT, state_.online_outcome().reason());
}

TEST_F(OnlineAttemptTest, HostedLoginRejected) {
  AuthFailure error(AuthFailure::FromNetworkAuthFailure(
      GoogleServiceAuthError(GoogleServiceAuthError::HOSTED_NOT_ALLOWED)));
  TestingProfile profile;

  base::RunLoop run_loop;
  EXPECT_CALL(resolver_, Resolve())
      .WillOnce(Invoke(&run_loop, &base::RunLoop::Quit))
      .RetiresOnSaturation();

  // This is how we inject fake URLFetcher objects, with a factory.
  MockURLFetcherFactory<HostedFetcher> factory;

  TestAttemptState local_state(UserContext(), true);
  attempt_.reset(new OnlineAttempt(&local_state, &resolver_));
  attempt_->Initiate(&profile);

  run_loop.Run();

  EXPECT_EQ(error, local_state.online_outcome());
  EXPECT_EQ(AuthFailure::NETWORK_AUTH_FAILED,
            local_state.online_outcome().reason());
}

TEST_F(OnlineAttemptTest, FullLogin) {
  TestingProfile profile;

  base::RunLoop run_loop;
  EXPECT_CALL(resolver_, Resolve())
      .WillOnce(Invoke(&run_loop, &base::RunLoop::Quit))
      .RetiresOnSaturation();

  // This is how we inject fake URLFetcher objects, with a factory.
  MockURLFetcherFactory<SuccessFetcher> factory;

  TestAttemptState local_state(UserContext(), true);
  attempt_.reset(new OnlineAttempt(&local_state, &resolver_));
  attempt_->Initiate(&profile);

  run_loop.Run();

  EXPECT_EQ(AuthFailure::AuthFailureNone(), local_state.online_outcome());
}

TEST_F(OnlineAttemptTest, LoginNetFailure) {
  RunFailureTest(
      GoogleServiceAuthError::FromConnectionError(net::ERR_CONNECTION_RESET));
}

TEST_F(OnlineAttemptTest, LoginDenied) {
  RunFailureTest(
      GoogleServiceAuthError(GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
}

TEST_F(OnlineAttemptTest, LoginAccountDisabled) {
  RunFailureTest(
      GoogleServiceAuthError(GoogleServiceAuthError::ACCOUNT_DISABLED));
}

TEST_F(OnlineAttemptTest, LoginAccountDeleted) {
  RunFailureTest(
      GoogleServiceAuthError(GoogleServiceAuthError::ACCOUNT_DELETED));
}

TEST_F(OnlineAttemptTest, LoginServiceUnavailable) {
  RunFailureTest(
      GoogleServiceAuthError(GoogleServiceAuthError::SERVICE_UNAVAILABLE));
}

TEST_F(OnlineAttemptTest, CaptchaErrorOutputted) {
  GoogleServiceAuthError auth_error =
      GoogleServiceAuthError::FromClientLoginCaptchaChallenge(
          "CCTOKEN",
          GURL("http://accounts.google.com/Captcha?ctoken=CCTOKEN"),
          GURL("http://www.google.com/login/captcha"));
  RunFailureTest(auth_error);
}

TEST_F(OnlineAttemptTest, TwoFactorSuccess) {
  EXPECT_CALL(resolver_, Resolve())
      .Times(1)
      .RetiresOnSaturation();
  GoogleServiceAuthError error(GoogleServiceAuthError::TWO_FACTOR);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&OnlineAttempt::OnClientLoginFailure,
                 attempt_->weak_factory_.GetWeakPtr(),
                 error));

  // Force UI thread to finish tasks so I can verify |state_|.
  base::RunLoop().RunUntilIdle();
  EXPECT_TRUE(GoogleServiceAuthError::AuthErrorNone() ==
              state_.online_outcome().error());
}

}  // namespace chromeos
