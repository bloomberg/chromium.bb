// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/message_loop.h"
#include "base/ref_counted.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/chromeos/cros/mock_library_loader.h"
#include "chrome/browser/chromeos/login/auth_attempt_state.h"
#include "chrome/browser/chromeos/login/online_attempt.h"
#include "chrome/browser/chromeos/login/mock_auth_attempt_state_resolver.h"
#include "chrome/browser/chromeos/login/mock_url_fetchers.h"
#include "chrome/browser/chromeos/login/test_attempt_state.h"
#include "chrome/common/net/gaia/gaia_auth_consumer.h"
#include "chrome/common/net/gaia/gaia_authenticator2_unittest.h"
#include "chrome/test/testing_profile.h"
#include "googleurl/src/gurl.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AnyNumber;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::_;

namespace chromeos {

class OnlineAttemptTest : public ::testing::Test {
 public:
  OnlineAttemptTest()
      : message_loop_(MessageLoop::TYPE_UI),
        ui_thread_(ChromeThread::UI, &message_loop_),
        io_thread_(ChromeThread::IO),
        state_("", "", "", "", ""),
        resolver_(new MockAuthAttemptStateResolver) {
  }

  virtual ~OnlineAttemptTest() {}

  virtual void SetUp() {
    CrosLibrary::TestApi* test_api = CrosLibrary::Get()->GetTestApi();

    MockLibraryLoader* loader = new MockLibraryLoader();
    ON_CALL(*loader, Load(_))
        .WillByDefault(Return(true));
    EXPECT_CALL(*loader, Load(_))
        .Times(AnyNumber());

    // Passes ownership of |loader| to CrosLibrary.
    test_api->SetLibraryLoader(loader, true);

    attempt_ = new OnlineAttempt(&state_, resolver_.get());

    io_thread_.Start();
  }

  virtual void TearDown() {
    // Prevent bogus gMock leak check from firing.
    chromeos::CrosLibrary::TestApi* test_api =
        chromeos::CrosLibrary::Get()->GetTestApi();
    test_api->SetLibraryLoader(NULL, false);
  }

  void RunFailureTest(const GoogleServiceAuthError& error) {
    EXPECT_CALL(*(resolver_.get()), Resolve())
        .Times(1)
        .RetiresOnSaturation();

    ChromeThread::PostTask(
        ChromeThread::IO, FROM_HERE,
        NewRunnableMethod(attempt_.get(),
                          &OnlineAttempt::OnClientLoginFailure,
                          error));
    // Force IO thread to finish tasks so I can verify |state_|.
    io_thread_.Stop();
    EXPECT_TRUE(error == state_.online_outcome().error());
  }

  void CancelLogin(OnlineAttempt* auth) {
    ChromeThread::PostTask(
        ChromeThread::IO,
        FROM_HERE,
        NewRunnableMethod(auth,
                          &OnlineAttempt::CancelClientLogin));
  }

  static void Quit() {
    ChromeThread::PostTask(
        ChromeThread::UI, FROM_HERE, new MessageLoop::QuitTask());
  }

  static void RunCancelTest(OnlineAttempt* attempt, Profile* profile) {
    attempt->Initiate(profile);
    MessageLoop::current()->RunAllPending();
  }

  MessageLoop message_loop_;
  ChromeThread ui_thread_;
  ChromeThread io_thread_;
  TestAttemptState state_;
  scoped_ptr<MockAuthAttemptStateResolver> resolver_;
  scoped_refptr<OnlineAttempt> attempt_;
};

TEST_F(OnlineAttemptTest, LoginSuccess) {
  GaiaAuthConsumer::ClientLoginResult result;
  EXPECT_CALL(*(resolver_.get()), Resolve())
      .Times(1)
      .RetiresOnSaturation();

  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(attempt_.get(),
                        &OnlineAttempt::OnClientLoginSuccess,
                        result));
  // Force IO thread to finish tasks so I can verify |state_|.
  io_thread_.Stop();
  EXPECT_TRUE(result == state_.credentials());
}

TEST_F(OnlineAttemptTest, LoginCancelRetry) {
  GoogleServiceAuthError error(GoogleServiceAuthError::REQUEST_CANCELED);
  TestingProfile profile;

  EXPECT_CALL(*(resolver_.get()), Resolve())
      .WillOnce(Invoke(OnlineAttemptTest::Quit))
      .RetiresOnSaturation();

  // This is how we inject fake URLFetcher objects, with a factory.
  // This factory creates fake URLFetchers that Start() a fake fetch attempt
  // and then come back on the IO thread saying they've been canceled.
  MockFactory<GotCanceledFetcher> factory;
  URLFetcher::set_factory(&factory);

  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableFunction(&OnlineAttemptTest::RunCancelTest,
                          attempt_.get(), &profile));

  MessageLoop::current()->Run();

  EXPECT_TRUE(error == state_.online_outcome().error());
  EXPECT_EQ(LoginFailure::NETWORK_AUTH_FAILED,
            state_.online_outcome().reason());
  URLFetcher::set_factory(NULL);
}

TEST_F(OnlineAttemptTest, LoginTimeout) {
  LoginFailure error(LoginFailure::LOGIN_TIMED_OUT);
  TestingProfile profile;

  EXPECT_CALL(*(resolver_.get()), Resolve())
      .WillOnce(Invoke(OnlineAttemptTest::Quit))
      .RetiresOnSaturation();

  // This is how we inject fake URLFetcher objects, with a factory.
  // This factory creates fake URLFetchers that Start() a fake fetch attempt
  // and then come back on the IO thread saying they've been canceled.
  MockFactory<ExpectCanceledFetcher> factory;
  URLFetcher::set_factory(&factory);

  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableFunction(&OnlineAttemptTest::RunCancelTest,
                          attempt_.get(), &profile));

  // Post a task to cancel the login attempt.
  CancelLogin(attempt_.get());

  MessageLoop::current()->Run();

  EXPECT_EQ(LoginFailure::LOGIN_TIMED_OUT, state_.online_outcome().reason());
  URLFetcher::set_factory(NULL);
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
      GoogleServiceAuthError::FromCaptchaChallenge(
          "CCTOKEN",
          GURL("http://www.google.com/accounts/Captcha?ctoken=CCTOKEN"),
          GURL("http://www.google.com/login/captcha"));
  RunFailureTest(auth_error);
}

TEST_F(OnlineAttemptTest, TwoFactorSuccess) {
  EXPECT_CALL(*(resolver_.get()), Resolve())
      .Times(1)
      .RetiresOnSaturation();
  GoogleServiceAuthError error(GoogleServiceAuthError::TWO_FACTOR);
  ChromeThread::PostTask(
      ChromeThread::IO, FROM_HERE,
      NewRunnableMethod(attempt_.get(),
                        &OnlineAttempt::OnClientLoginFailure,
                        error));

  // Force IO thread to finish tasks so I can verify |state_|.
  io_thread_.Stop();
  EXPECT_TRUE(GoogleServiceAuthError::None() ==
              state_.online_outcome().error());
  EXPECT_TRUE(GaiaAuthConsumer::ClientLoginResult() == state_.credentials());
}

}  // namespace chromeos
