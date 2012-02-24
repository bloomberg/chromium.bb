// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/signin/signin_tracker.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/compiler_specific.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/signin_manager.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/net/gaia/google_service_auth_error.h"
#include "content/public/browser/notification_service.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::Mock;
using ::testing::Return;
using ::testing::ReturnRef;

class MockObserver : public SigninTracker::Observer {
 public:
  MockObserver() {}
  ~MockObserver() {}

  MOCK_METHOD0(GaiaCredentialsValid, void(void));
  MOCK_METHOD0(SigninFailed, void(void));
  MOCK_METHOD0(SigninSuccess, void(void));
};

class SigninTrackerTest : public testing::Test {
 public:
  SigninTrackerTest() {}
  virtual void SetUp() OVERRIDE {
    profile_.reset(ProfileSyncServiceMock::MakeSignedInTestingProfile());
    mock_pss_ = static_cast<ProfileSyncServiceMock*>(
        ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile_.get(),
            ProfileSyncServiceMock::BuildMockProfileSyncService));
    tracker_.reset(new SigninTracker(profile_.get(), &observer_));
  }
  virtual void TearDown() OVERRIDE {
    tracker_.reset();
    profile_.reset();
  }
  scoped_ptr<SigninTracker> tracker_;
  scoped_ptr<Profile> profile_;
  ProfileSyncServiceMock* mock_pss_;
  MockObserver observer_;
};

TEST_F(SigninTrackerTest, GaiaSignInFailed) {
  // SIGNIN_FAILED notification should result in a SigninFailed callback.
  EXPECT_CALL(observer_, SigninFailed());
  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_GOOGLE_SIGNIN_FAILED,
      content::Source<Profile>(profile_.get()),
      content::Details<const GoogleServiceAuthError>(&error));
}

TEST_F(SigninTrackerTest, GaiaSignInSucceeded) {
  // SIGNIN_SUCCEEDED notification should lead us to get a GaiCredentialsValid()
  // callback.
  EXPECT_CALL(observer_, GaiaCredentialsValid());
  EXPECT_CALL(*mock_pss_, AreCredentialsAvailable())
      .WillRepeatedly(Return(false));
  GoogleServiceSigninSuccessDetails details("username@gmail.com", "password");
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL,
      content::Source<Profile>(profile_.get()),
      content::Details<const GoogleServiceSigninSuccessDetails>(&details));
}

static void ExpectSignedInSyncService(ProfileSyncServiceMock* mock,
                                      const GoogleServiceAuthError& error) {
  EXPECT_CALL(*mock, AreCredentialsAvailable()).WillRepeatedly(
      Return(true));
  EXPECT_CALL(*mock, waiting_for_auth()).WillRepeatedly(Return(false));
  EXPECT_CALL(*mock, GetAuthError()).WillRepeatedly(ReturnRef(error));
  EXPECT_CALL(*mock, unrecoverable_error_detected()).WillRepeatedly(
      Return(false));
  EXPECT_CALL(*mock, sync_initialized()).WillRepeatedly(Return(true));

}

TEST_F(SigninTrackerTest, GaiaSigninWhenServicesAlreadyRunning) {
  // SIGNIN_SUCCEEDED notification should result in a SigninSuccess() callback
  // if we're already signed in.
  EXPECT_CALL(observer_, GaiaCredentialsValid());
  EXPECT_CALL(observer_, SigninSuccess());
  GoogleServiceAuthError error(GoogleServiceAuthError::NONE);
  ExpectSignedInSyncService(mock_pss_, error);
  GoogleServiceSigninSuccessDetails details("username@gmail.com", "password");
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL,
      content::Source<Profile>(profile_.get()),
      content::Details<const GoogleServiceSigninSuccessDetails>(&details));
}

TEST_F(SigninTrackerTest, NoGaiaSigninWhenServicesNotRunning) {
  // SIGNIN_SUCCEEDED notification should not result in a SigninSuccess()
  // callback if we're not already signed in.
  EXPECT_CALL(observer_, GaiaCredentialsValid());
  EXPECT_CALL(*mock_pss_, AreCredentialsAvailable()).WillRepeatedly(
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
  EXPECT_CALL(*mock_pss_, AreCredentialsAvailable()).WillOnce(
      Return(false));
  GoogleServiceSigninSuccessDetails details("username@gmail.com", "password");
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_GOOGLE_SIGNIN_SUCCESSFUL,
      content::Source<Profile>(profile_.get()),
      content::Details<const GoogleServiceSigninSuccessDetails>(&details));
  Mock::VerifyAndClearExpectations(mock_pss_);
  // Mimic the sync engine getting credentials.
  EXPECT_CALL(observer_, SigninSuccess());
  GoogleServiceAuthError error(GoogleServiceAuthError::NONE);
  ExpectSignedInSyncService(mock_pss_, error);
  tracker_->OnStateChanged();
}

TEST_F(SigninTrackerTest, SyncSigninError) {
  // Make sure that we get a SigninFailed() callback if sync gets an error after
  // initializaiton.
  EXPECT_CALL(observer_, GaiaCredentialsValid());
  EXPECT_CALL(*mock_pss_, AreCredentialsAvailable()).WillRepeatedly(
      Return(false));
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
  EXPECT_CALL(*mock_pss_, waiting_for_auth()).WillOnce(Return(false));
  EXPECT_CALL(observer_, SigninFailed());
  tracker_->OnStateChanged();
}

