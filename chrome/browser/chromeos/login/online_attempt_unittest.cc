// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/bind.h"
#include "base/memory/ref_counted.h"
#include "base/message_loop.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/login/auth_attempt_state.h"
#include "chrome/browser/chromeos/login/mock_auth_attempt_state_resolver.h"
#include "chrome/browser/chromeos/login/mock_url_fetchers.h"
#include "chrome/browser/chromeos/login/online_attempt.h"
#include "chrome/browser/chromeos/login/test_attempt_state.h"
#include "chrome/browser/chromeos/login/user.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread.h"
#include "google_apis/gaia/gaia_auth_consumer.h"
#include "google_apis/gaia/mock_url_fetcher_factory.h"
#include "googleurl/src/gurl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::_;
using content::BrowserThread;

namespace chromeos {

class OnlineAttemptTest : public testing::Test {
 public:
  OnlineAttemptTest()
      : message_loop_(MessageLoop::TYPE_UI),
        ui_thread_(BrowserThread::UI, &message_loop_),
        state_(UserCredentials(), "", "", "", User::USER_TYPE_REGULAR, false),
        resolver_(new MockAuthAttemptStateResolver) {
  }

  virtual ~OnlineAttemptTest() {}

  virtual void SetUp() {
    attempt_.reset(new OnlineAttempt(&state_, resolver_.get()));
  }

  virtual void TearDown() {
  }

  void RunFailureTest(const GoogleServiceAuthError& error) {
    EXPECT_CALL(*(resolver_.get()), Resolve())
        .Times(1)
        .RetiresOnSaturation();

    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&OnlineAttempt::OnClientLoginFailure,
                   attempt_->weak_factory_.GetWeakPtr(),
                   error));
    // Force UI thread to finish tasks so I can verify |state_|.
    message_loop_.RunUntilIdle();
    EXPECT_TRUE(error == state_.online_outcome().error());
  }

  void CancelLogin(OnlineAttempt* auth) {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE,
        base::Bind(&OnlineAttempt::CancelClientLogin,
                   auth->weak_factory_.GetWeakPtr()));
  }

  static void Quit() {
    BrowserThread::PostTask(
        BrowserThread::UI, FROM_HERE, MessageLoop::QuitClosure());
  }

  static void RunThreadTest() {
    MessageLoop::current()->RunUntilIdle();
  }

  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;
  TestAttemptState state_;
  scoped_ptr<MockAuthAttemptStateResolver> resolver_;
  scoped_ptr<OnlineAttempt> attempt_;

  // Initializes / shuts down a stub CrosLibrary.
  chromeos::ScopedStubCrosEnabler stub_cros_enabler_;
};

TEST_F(OnlineAttemptTest, LoginSuccess) {
  EXPECT_CALL(*(resolver_.get()), Resolve())
      .Times(1)
      .RetiresOnSaturation();

  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&OnlineAttempt::OnClientLoginSuccess,
                 attempt_->weak_factory_.GetWeakPtr(),
                 GaiaAuthConsumer::ClientLoginResult()));
  // Force UI thread to finish tasks so I can verify |state_|.
  message_loop_.RunUntilIdle();
}

TEST_F(OnlineAttemptTest, LoginCancelRetry) {
  GoogleServiceAuthError error(GoogleServiceAuthError::REQUEST_CANCELED);
  TestingProfile profile;

  EXPECT_CALL(*(resolver_.get()), Resolve())
      .WillOnce(Invoke(OnlineAttemptTest::Quit))
      .RetiresOnSaturation();

  // This is how we inject fake URLFetcher objects, with a factory.
  // This factory creates fake URLFetchers that Start() a fake fetch attempt
  // and then come back on the UI thread saying they've been canceled.
  MockURLFetcherFactory<GotCanceledFetcher> factory;

  attempt_->Initiate(&profile);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&OnlineAttemptTest::RunThreadTest));

  MessageLoop::current()->Run();

  EXPECT_TRUE(error == state_.online_outcome().error());
  EXPECT_EQ(LoginFailure::NETWORK_AUTH_FAILED,
            state_.online_outcome().reason());
}

TEST_F(OnlineAttemptTest, LoginTimeout) {
  LoginFailure error(LoginFailure::LOGIN_TIMED_OUT);
  TestingProfile profile;

  EXPECT_CALL(*(resolver_.get()), Resolve())
      .WillOnce(Invoke(OnlineAttemptTest::Quit))
      .RetiresOnSaturation();

  // This is how we inject fake URLFetcher objects, with a factory.
  // This factory creates fake URLFetchers that Start() a fake fetch attempt
  // and then come back on the UI thread saying they've been canceled.
  MockURLFetcherFactory<ExpectCanceledFetcher> factory;

  attempt_->Initiate(&profile);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&OnlineAttemptTest::RunThreadTest));

  // Post a task to cancel the login attempt.
  CancelLogin(attempt_.get());

  MessageLoop::current()->Run();

  EXPECT_EQ(LoginFailure::LOGIN_TIMED_OUT, state_.online_outcome().reason());
}

TEST_F(OnlineAttemptTest, HostedLoginRejected) {
  LoginFailure error(
      LoginFailure::FromNetworkAuthFailure(
          GoogleServiceAuthError(
              GoogleServiceAuthError::HOSTED_NOT_ALLOWED)));
  TestingProfile profile;

  EXPECT_CALL(*(resolver_.get()), Resolve())
      .WillOnce(Invoke(OnlineAttemptTest::Quit))
      .RetiresOnSaturation();

  // This is how we inject fake URLFetcher objects, with a factory.
  MockURLFetcherFactory<HostedFetcher> factory;

  TestAttemptState local_state(UserCredentials(), "", "", "",
                               User::USER_TYPE_REGULAR, true);
  attempt_.reset(new OnlineAttempt(&local_state, resolver_.get()));
  attempt_->Initiate(&profile);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&OnlineAttemptTest::RunThreadTest));

  MessageLoop::current()->Run();

  EXPECT_EQ(error, local_state.online_outcome());
  EXPECT_EQ(LoginFailure::NETWORK_AUTH_FAILED,
            local_state.online_outcome().reason());
}

TEST_F(OnlineAttemptTest, FullLogin) {
  TestingProfile profile;

  EXPECT_CALL(*(resolver_.get()), Resolve())
      .WillOnce(Invoke(OnlineAttemptTest::Quit))
      .RetiresOnSaturation();

  // This is how we inject fake URLFetcher objects, with a factory.
  MockURLFetcherFactory<SuccessFetcher> factory;

  TestAttemptState local_state(UserCredentials(), "", "", "",
                               User::USER_TYPE_REGULAR, true);
  attempt_.reset(new OnlineAttempt(&local_state, resolver_.get()));
  attempt_->Initiate(&profile);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&OnlineAttemptTest::RunThreadTest));

  MessageLoop::current()->Run();

  EXPECT_EQ(LoginFailure::LoginFailureNone(), local_state.online_outcome());
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
  EXPECT_CALL(*(resolver_.get()), Resolve())
      .Times(1)
      .RetiresOnSaturation();
  GoogleServiceAuthError error(GoogleServiceAuthError::TWO_FACTOR);
  BrowserThread::PostTask(
      BrowserThread::UI, FROM_HERE,
      base::Bind(&OnlineAttempt::OnClientLoginFailure,
                 attempt_->weak_factory_.GetWeakPtr(),
                 error));

  // Force UI thread to finish tasks so I can verify |state_|.
  message_loop_.RunUntilIdle();
  EXPECT_TRUE(GoogleServiceAuthError::AuthErrorNone() ==
              state_.online_outcome().error());
}

}  // namespace chromeos
