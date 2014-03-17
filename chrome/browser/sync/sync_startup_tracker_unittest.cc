// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/sync/profile_sync_service_factory.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/browser/sync/sync_startup_tracker.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::_;
using ::testing::AnyNumber;
using ::testing::Mock;
using ::testing::Return;
using ::testing::ReturnRef;

namespace {

class MockObserver : public SyncStartupTracker::Observer {
 public:
  MOCK_METHOD0(SyncStartupCompleted, void(void));
  MOCK_METHOD0(SyncStartupFailed, void(void));
};

class SyncStartupTrackerTest : public testing::Test {
 public:
  SyncStartupTrackerTest() :
      no_error_(GoogleServiceAuthError::NONE) {
  }
  virtual void SetUp() OVERRIDE {
    profile_.reset(new TestingProfile());
    mock_pss_ = static_cast<ProfileSyncServiceMock*>(
        ProfileSyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            profile_.get(),
            ProfileSyncServiceMock::BuildMockProfileSyncService));

    // Make gmock not spam the output with information about these uninteresting
    // calls.
    EXPECT_CALL(*mock_pss_, AddObserver(_)).Times(AnyNumber());
    EXPECT_CALL(*mock_pss_, RemoveObserver(_)).Times(AnyNumber());
    EXPECT_CALL(*mock_pss_, GetAuthError()).
        WillRepeatedly(ReturnRef(no_error_));
    ON_CALL(*mock_pss_, GetRegisteredDataTypes())
        .WillByDefault(Return(syncer::ModelTypeSet()));
    mock_pss_->Initialize();
  }

  virtual void TearDown() OVERRIDE {
    profile_.reset();
  }

  void SetupNonInitializedPSS() {
    EXPECT_CALL(*mock_pss_, GetAuthError())
        .WillRepeatedly(ReturnRef(no_error_));
    EXPECT_CALL(*mock_pss_, sync_initialized()).WillRepeatedly(Return(false));
    EXPECT_CALL(*mock_pss_, HasUnrecoverableError())
        .WillRepeatedly(Return(false));
    EXPECT_CALL(*mock_pss_, IsSyncEnabledAndLoggedIn())
        .WillRepeatedly(Return(true));
  }

  content::TestBrowserThreadBundle thread_bundle_;
  GoogleServiceAuthError no_error_;
  scoped_ptr<TestingProfile> profile_;
  ProfileSyncServiceMock* mock_pss_;
  MockObserver observer_;
};

TEST_F(SyncStartupTrackerTest, SyncAlreadyInitialized) {
  EXPECT_CALL(*mock_pss_, sync_initialized()).WillRepeatedly(Return(true));
  EXPECT_CALL(*mock_pss_, IsSyncEnabledAndLoggedIn())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(observer_, SyncStartupCompleted());
  SyncStartupTracker tracker(profile_.get(), &observer_);
}

TEST_F(SyncStartupTrackerTest, SyncNotSignedIn) {
  // Make sure that we get a SyncStartupFailed() callback if sync is not logged
  // in.
  EXPECT_CALL(*mock_pss_, sync_initialized()).WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, IsSyncEnabledAndLoggedIn()).WillRepeatedly(
      Return(false));
  EXPECT_CALL(observer_, SyncStartupFailed());
  SyncStartupTracker tracker(profile_.get(), &observer_);
}

TEST_F(SyncStartupTrackerTest, SyncAuthError) {
  // Make sure that we get a SyncStartupFailed() callback if sync gets an auth
  // error.
  EXPECT_CALL(*mock_pss_, sync_initialized()).WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, IsSyncEnabledAndLoggedIn()).WillRepeatedly(
      Return(true));
  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  EXPECT_CALL(*mock_pss_, GetAuthError()).WillRepeatedly(ReturnRef(error));
  EXPECT_CALL(observer_, SyncStartupFailed());
  SyncStartupTracker tracker(profile_.get(), &observer_);
}

TEST_F(SyncStartupTrackerTest, SyncDelayedInitialization) {
  // Non-initialized PSS should result in no callbacks to the observer.
  SetupNonInitializedPSS();
  EXPECT_CALL(observer_, SyncStartupCompleted()).Times(0);
  EXPECT_CALL(observer_, SyncStartupFailed()).Times(0);
  SyncStartupTracker tracker(profile_.get(), &observer_);
  Mock::VerifyAndClearExpectations(&observer_);
  // Now, mark the PSS as initialized.
  EXPECT_CALL(*mock_pss_, sync_initialized()).WillRepeatedly(Return(true));
  EXPECT_CALL(observer_, SyncStartupCompleted());
  tracker.OnStateChanged();
}

TEST_F(SyncStartupTrackerTest, SyncDelayedAuthError) {
  // Non-initialized PSS should result in no callbacks to the observer.
  SetupNonInitializedPSS();
  EXPECT_CALL(observer_, SyncStartupCompleted()).Times(0);
  EXPECT_CALL(observer_, SyncStartupFailed()).Times(0);
  SyncStartupTracker tracker(profile_.get(), &observer_);
  Mock::VerifyAndClearExpectations(&observer_);
  Mock::VerifyAndClearExpectations(mock_pss_);

  // Now, mark the PSS as having an auth error.
  EXPECT_CALL(*mock_pss_, sync_initialized()).WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, IsSyncEnabledAndLoggedIn()).WillRepeatedly(
      Return(true));
  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  EXPECT_CALL(*mock_pss_, GetAuthError()).WillRepeatedly(ReturnRef(error));
  EXPECT_CALL(observer_, SyncStartupFailed());
  tracker.OnStateChanged();
}

TEST_F(SyncStartupTrackerTest, SyncDelayedUnrecoverableError) {
  // Non-initialized PSS should result in no callbacks to the observer.
  SetupNonInitializedPSS();
  EXPECT_CALL(observer_, SyncStartupCompleted()).Times(0);
  EXPECT_CALL(observer_, SyncStartupFailed()).Times(0);
  SyncStartupTracker tracker(profile_.get(), &observer_);
  Mock::VerifyAndClearExpectations(&observer_);
  Mock::VerifyAndClearExpectations(mock_pss_);

  // Now, mark the PSS as having an unrecoverable error.
  EXPECT_CALL(*mock_pss_, sync_initialized()).WillRepeatedly(Return(false));
  EXPECT_CALL(*mock_pss_, IsSyncEnabledAndLoggedIn()).WillRepeatedly(
      Return(true));
  GoogleServiceAuthError error(
      GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS);
  EXPECT_CALL(*mock_pss_, GetAuthError()).WillRepeatedly(ReturnRef(error));
  EXPECT_CALL(observer_, SyncStartupFailed());
  tracker.OnStateChanged();
}

}  // namespace
