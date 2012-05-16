// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_tracker.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/signin/token_service.h"
#include "chrome/browser/signin/token_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "content/public/browser/notification_service.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Mock;
using ::testing::Return;
using ::testing::ReturnRef;

class MockTokenService : public TokenService {
 public:
  MockTokenService() { }
  virtual ~MockTokenService() { }

  MOCK_CONST_METHOD1(HasTokenForService, bool(const char*));
};

ProfileKeyedService* BuildMockTokenService(Profile* profile) {
  return new MockTokenService;
}

class MockObserver : public SigninTracker::Observer {
 public:
  MockObserver() {}
  ~MockObserver() {}

  MOCK_METHOD0(GaiaCredentialsValid, void(void));
  MOCK_METHOD1(SigninFailed, void(const GoogleServiceAuthError&));
  MOCK_METHOD0(SigninSuccess, void(void));
};

class SigninTrackerTest : public testing::Test {
 public:
  SigninTrackerTest() {}
  virtual void SetUp() OVERRIDE {
    profile_.reset(ProfileSyncServiceMock::MakeSignedInTestingProfile());
    mock_token_service_ = static_cast<MockTokenService*>(
        TokenServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile_.get(), BuildMockTokenService));
    mock_pss_ = static_cast<ProfileSyncServiceMock*>(
        ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile_.get(),
            ProfileSyncServiceMock::BuildMockProfileSyncService));
    // Make gmock not spam the output with information about these uninteresting
    // calls.
    EXPECT_CALL(*mock_pss_, AddObserver(_)).Times(AnyNumber());
    EXPECT_CALL(*mock_pss_, RemoveObserver(_)).Times(AnyNumber());
    tracker_.reset(new SigninTracker(profile_.get(), &observer_));
  }
  virtual void TearDown() OVERRIDE {
    tracker_.reset();
    profile_.reset();
  }
  scoped_ptr<SigninTracker> tracker_;
  scoped_ptr<TestingProfile> profile_;
  ProfileSyncServiceMock* mock_pss_;
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
  // SIGNIN_SUCCEEDED notification should lead us to get a GaiCredentialsValid()
  // callback.
  EXPECT_CALL(observer_, GaiaCredentialsValid());
  EXPECT_CALL(*mock_pss_, IsSyncEnabledAndLoggedIn())
      .WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_token_service_, HasTokenForService(_))
      .WillRepeatedly(Return(false));
  GoogleServiceSigninSuccessDetails details("username@gmail.com", "password");
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL,
      content::Source<Profile>(profile_.get()),
      content::Details<const GoogleServiceSigninSuccessDetails>(&details));
}

static void ExpectSignedInSyncService(ProfileSyncServiceMock* sync_service,
                                      MockTokenService* token_service,
                                      const GoogleServiceAuthError& error) {
  if (token_service) {
    EXPECT_CALL(*token_service, HasTokenForService(_))
        .WillRepeatedly(Return(true));
  }
  EXPECT_CALL(*sync_service, IsSyncEnabledAndLoggedIn()).WillRepeatedly(
      Return(true));
  EXPECT_CALL(*sync_service, IsSyncTokenAvailable()).WillRepeatedly(
      Return(true));
  EXPECT_CALL(*sync_service, waiting_for_auth()).WillRepeatedly(Return(false));
  EXPECT_CALL(*sync_service, GetAuthError()).WillRepeatedly(ReturnRef(error));
  EXPECT_CALL(*sync_service, unrecoverable_error_detected()).WillRepeatedly(
      Return(false));
  EXPECT_CALL(*sync_service, sync_initialized()).WillRepeatedly(Return(true));

}

TEST_F(SigninTrackerTest, GaiaSigninWhenServicesAlreadyRunning) {
  // SIGNIN_SUCCEEDED notification should result in a SigninSuccess() callback
  // if we're already signed in.
  EXPECT_CALL(observer_, GaiaCredentialsValid());
  EXPECT_CALL(observer_, SigninSuccess());
  GoogleServiceAuthError error(GoogleServiceAuthError::NONE);
  ExpectSignedInSyncService(mock_pss_, mock_token_service_, error);
  GoogleServiceSigninSuccessDetails details("username@gmail.com", "password");
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL,
      content::Source<Profile>(profile_.get()),
      content::Details<const GoogleServiceSigninSuccessDetails>(&details));
}

TEST_F(SigninTrackerTest, NoGaiaSigninWhenOAuthTokensNotAvailable) {
  // SIGNIN_SUCCESSFUL notification should not result in a SigninSuccess()
  // callback if our oauth token hasn't been fetched.
  EXPECT_CALL(observer_, GaiaCredentialsValid());
  GoogleServiceAuthError error(GoogleServiceAuthError::NONE);
  ExpectSignedInSyncService(mock_pss_, NULL, error);
  EXPECT_CALL(*mock_token_service_,
              HasTokenForService(GaiaConstants::kSyncService))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_token_service_,
              HasTokenForService(GaiaConstants::kGaiaOAuth2LoginRefreshToken))
      .WillRepeatedly(Return(false));
  GoogleServiceSigninSuccessDetails details("username@gmail.com", "password");
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL,
      content::Source<Profile>(profile_.get()),
      content::Details<const GoogleServiceSigninSuccessDetails>(&details));
}

TEST_F(SigninTrackerTest, GaiaSigninAfterOAuthTokenBecomesAvailable) {
  // SIGNIN_SUCCESSFUL notification should not result in a SigninSuccess()
  // callback until after our oauth token hasn't been fetched.
  EXPECT_CALL(observer_, GaiaCredentialsValid());
  GoogleServiceAuthError none(GoogleServiceAuthError::NONE);
  ExpectSignedInSyncService(mock_pss_, NULL, none);
  EXPECT_CALL(*mock_token_service_,
              HasTokenForService(GaiaConstants::kSyncService))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_token_service_,
              HasTokenForService(GaiaConstants::kGaiaOAuth2LoginRefreshToken))
      .WillRepeatedly(Return(false));
  GoogleServiceSigninSuccessDetails details("username@gmail.com", "password");
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL,
      content::Source<Profile>(profile_.get()),
      content::Details<const GoogleServiceSigninSuccessDetails>(&details));
  Mock::VerifyAndClearExpectations(mock_pss_);
  Mock::VerifyAndClearExpectations(mock_token_service_);
  EXPECT_CALL(observer_, SigninSuccess());
  ExpectSignedInSyncService(mock_pss_, mock_token_service_, none);
  TokenService::TokenAvailableDetails available(
      GaiaConstants::kGaiaOAuth2LoginRefreshToken, "foo_token");
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_TOKEN_AVAILABLE,
      content::Source<TokenService>(mock_token_service_),
      content::Details<const TokenService::TokenAvailableDetails>(&available));
}

TEST_F(SigninTrackerTest, SigninFailedWhenTokenFetchFails) {
  // TOKEN_REQUEST_FAILED notification should result in SigninFailed() callback.
  EXPECT_CALL(observer_, GaiaCredentialsValid());
  GoogleServiceAuthError none(GoogleServiceAuthError::NONE);
  ExpectSignedInSyncService(mock_pss_, NULL, none);
  EXPECT_CALL(*mock_token_service_,
              HasTokenForService(GaiaConstants::kSyncService))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_token_service_,
              HasTokenForService(GaiaConstants::kGaiaOAuth2LoginRefreshToken))
      .WillRepeatedly(Return(false));
  GoogleServiceSigninSuccessDetails details("username@gmail.com", "password");
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL,
      content::Source<Profile>(profile_.get()),
      content::Details<const GoogleServiceSigninSuccessDetails>(&details));

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

TEST_F(SigninTrackerTest, NoGaiaSigninWhenServicesNotRunning) {
  // SIGNIN_SUCCEEDED notification should not result in a SigninSuccess()
  // callback if we're not already signed in.
  EXPECT_CALL(observer_, GaiaCredentialsValid());
  EXPECT_CALL(*mock_token_service_, HasTokenForService(_))
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_pss_, IsSyncEnabledAndLoggedIn()).WillRepeatedly(
      Return(false));
  EXPECT_CALL(*mock_pss_, IsSyncTokenAvailable()).WillRepeatedly(
      Return(false));
  GoogleServiceSigninSuccessDetails details("username@gmail.com", "password");
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL,
      content::Source<Profile>(profile_.get()),
      content::Details<const GoogleServiceSigninSuccessDetails>(&details));
}

TEST_F(SigninTrackerTest, GaiaSigninAfterSyncStarts) {
  // Make sure that we don't get a SigninSuccess() callback until after the
  // sync service reports that it's signed in.
  EXPECT_CALL(observer_, GaiaCredentialsValid());
  EXPECT_CALL(*mock_pss_, IsSyncEnabledAndLoggedIn()).WillOnce(
      Return(false));
  EXPECT_CALL(*mock_token_service_, HasTokenForService(_))
      .WillRepeatedly(Return(true));
  GoogleServiceSigninSuccessDetails details("username@gmail.com", "password");
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL,
      content::Source<Profile>(profile_.get()),
      content::Details<const GoogleServiceSigninSuccessDetails>(&details));
  Mock::VerifyAndClearExpectations(mock_pss_);
  // Mimic the sync engine getting credentials.
  EXPECT_CALL(observer_, SigninSuccess());
  GoogleServiceAuthError error(GoogleServiceAuthError::NONE);
  ExpectSignedInSyncService(mock_pss_, mock_token_service_, error);
  tracker_->OnStateChanged();
}

TEST_F(SigninTrackerTest, SyncSigninError) {
  // Make sure that we get a SigninFailed() callback if sync gets an error after
  // initializaiton.
  EXPECT_CALL(observer_, GaiaCredentialsValid());
  EXPECT_CALL(*mock_pss_, IsSyncEnabledAndLoggedIn()).WillRepeatedly(
      Return(false));
  EXPECT_CALL(*mock_pss_, IsSyncTokenAvailable()).WillRepeatedly(
      Return(false));
  EXPECT_CALL(*mock_token_service_, HasTokenForService(_))
      .WillRepeatedly(Return(true));
  GoogleServiceSigninSuccessDetails details("username@gmail.com", "password");
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL,
      content::Source<Profile>(profile_.get()),
      content::Details<const GoogleServiceSigninSuccessDetails>(&details));

  // Still waiting for auth, so sync state changes should be ignored.
  EXPECT_CALL(*mock_pss_, waiting_for_auth()).WillOnce(Return(true));
  tracker_->OnStateChanged();
  // Now mimic an auth error - this should cause us to fail (not waiting for
  // auth, but still have no credentials).
  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  EXPECT_CALL(*mock_pss_, GetAuthError()).WillRepeatedly(ReturnRef(error));
  EXPECT_CALL(*mock_pss_, waiting_for_auth()).WillOnce(Return(false));
  EXPECT_CALL(observer_, SigninFailed(error));
  tracker_->OnStateChanged();
}

// TODO(kochi) Add tests with initial state != WATIING_FOR_GAIA_VALIDATION.
