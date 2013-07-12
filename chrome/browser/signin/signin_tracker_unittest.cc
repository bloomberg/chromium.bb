// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_tracker.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/fake_auth_status_provider.h"
#include "chrome/browser/signin/fake_signin_manager.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/google_service_auth_error.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Mock;
using ::testing::Return;
using ::testing::ReturnRef;

namespace {

class MockTokenService : public TokenService {
 public:
  MockTokenService() { }
  virtual ~MockTokenService() { }

  MOCK_CONST_METHOD1(HasTokenForService, bool(const char*));
};

BrowserContextKeyedService* BuildMockTokenService(
    content::BrowserContext* profile) {
  return new MockTokenService;
}

class MockObserver : public SigninTracker::Observer {
 public:
  MockObserver() {}
  ~MockObserver() {}

  MOCK_METHOD1(SigninFailed, void(const GoogleServiceAuthError&));
  MOCK_METHOD0(SigninSuccess, void(void));
};

}  // namespace

class SigninTrackerTest : public testing::Test {
 public:
  SigninTrackerTest() {}
  virtual void SetUp() OVERRIDE {
    profile_.reset(new TestingProfile());
    mock_token_service_ = static_cast<MockTokenService*>(
        TokenServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile_.get(), BuildMockTokenService));

    mock_signin_manager_ = static_cast<FakeSigninManagerBase*>(
        SigninManagerFactory::GetInstance()->SetTestingFactoryAndUse(
            profile_.get(), FakeSigninManagerBase::Build));
    mock_signin_manager_->Initialize(profile_.get(), NULL);
    tracker_.reset(new SigninTracker(profile_.get(), &observer_));
  }
  virtual void TearDown() OVERRIDE {
    tracker_.reset();
    profile_.reset();
  }

  void SendSigninSuccessful() {
    mock_signin_manager_->SetAuthenticatedUsername("username@gmail.com");
    GoogleServiceSigninSuccessDetails details("username@gmail.com", "password");
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL,
        content::Source<Profile>(profile_.get()),
        content::Details<const GoogleServiceSigninSuccessDetails>(&details));
  }

  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<SigninTracker> tracker_;
  scoped_ptr<TestingProfile> profile_;
  FakeSigninManagerBase* mock_signin_manager_;
  MockTokenService* mock_token_service_;
  MockObserver observer_;
};

TEST_F(SigninTrackerTest, GaiaSignInFailed) {
  // SIGNIN_FAILED notification should result in a SigninFailed callback.
  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  EXPECT_CALL(observer_, SigninFailed(error));
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_GOOGLE_SIGNIN_FAILED,
      content::Source<Profile>(profile_.get()),
      content::Details<const GoogleServiceAuthError>(&error));
}

TEST_F(SigninTrackerTest, GaiaSignInSucceeded) {
  // SIGNIN_SUCCEEDED notification should lead us to get a SigninSuccess()
  // callback.
  EXPECT_CALL(observer_, SigninSuccess());
  EXPECT_CALL(*mock_token_service_, HasTokenForService(_))
      .WillRepeatedly(Return(true));
  SendSigninSuccessful();
}

TEST_F(SigninTrackerTest, NoGaiaSigninWhenOAuthTokensNotAvailable) {
  // SIGNIN_SUCCESSFUL notification should not result in a SigninSuccess()
  // callback if our oauth token hasn't been fetched.
  EXPECT_CALL(observer_, SigninSuccess()).Times(0);
  EXPECT_CALL(observer_, SigninFailed(_)).Times(0);
  EXPECT_CALL(*mock_token_service_,
              HasTokenForService(GaiaConstants::kSyncService))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_token_service_,
              HasTokenForService(GaiaConstants::kGaiaOAuth2LoginRefreshToken))
      .WillRepeatedly(Return(false));
  SendSigninSuccessful();
}

TEST_F(SigninTrackerTest, GaiaSigninAfterOAuthTokenBecomesAvailable) {
  // SIGNIN_SUCCESSFUL notification should not result in a SigninSuccess()
  // callback until after our oauth token has been fetched.
  EXPECT_CALL(observer_, SigninSuccess()).Times(0);
  EXPECT_CALL(*mock_token_service_,
              HasTokenForService(GaiaConstants::kSyncService))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_token_service_,
              HasTokenForService(GaiaConstants::kGaiaOAuth2LoginRefreshToken))
      .WillRepeatedly(Return(false));
  SendSigninSuccessful();
  Mock::VerifyAndClearExpectations(mock_token_service_);
  EXPECT_CALL(observer_, SigninSuccess());
  TokenService::TokenAvailableDetails available(
      GaiaConstants::kGaiaOAuth2LoginRefreshToken, "foo_token");
  EXPECT_CALL(*mock_token_service_, HasTokenForService(_))
      .WillRepeatedly(Return(true));
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_TOKEN_AVAILABLE,
      content::Source<TokenService>(mock_token_service_),
      content::Details<const TokenService::TokenAvailableDetails>(&available));
}

TEST_F(SigninTrackerTest, SigninFailedWhenTokenFetchFails) {
  // TOKEN_REQUEST_FAILED notification should result in SigninFailed() callback.
  // We should not get any SigninFailed() callbacks until we issue the
  // TOKEN_REQUEST_FAILED notification.
  EXPECT_CALL(observer_, SigninFailed(_)).Times(0);
  EXPECT_CALL(*mock_token_service_,
              HasTokenForService(GaiaConstants::kSyncService))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_token_service_,
              HasTokenForService(GaiaConstants::kGaiaOAuth2LoginRefreshToken))
      .WillRepeatedly(Return(false));
  SendSigninSuccessful();

  Mock::VerifyAndClearExpectations(&observer_);
  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  EXPECT_CALL(observer_, SigninFailed(error));
  TokenService::TokenRequestFailedDetails failed(
      GaiaConstants::kGaiaOAuth2LoginRefreshToken, error);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_TOKEN_REQUEST_FAILED,
      content::Source<TokenService>(mock_token_service_),
      content::Details<const TokenService::TokenRequestFailedDetails>(&failed));
}
