// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/glue/data_type_controller_mock.h"
#include "chrome/browser/sync/test_profile_sync_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/testing_profile.h"
#include "testing/gmock/include/gmock/gmock.h"

using browser_sync::DataTypeController;
using browser_sync::DataTypeControllerMock;
using testing::_;
using testing::InvokeArgument;
using testing::Mock;
using testing::Return;

ACTION_P(InvokeCallback, callback_result) {
  arg1->Run(callback_result);
  delete arg1;
}

// TODO(skrul) This test fails on the mac. See http://crbug.com/33443
#if defined(OS_MACOSX)
#define SKIP_MACOSX(test) DISABLED_##test
#else
#define SKIP_MACOSX(test) test
#endif

class ProfileSyncServiceObserverMock : public ProfileSyncServiceObserver {
 public:
  MOCK_METHOD0(OnStateChanged, void());
};

class ProfileSyncServiceStartupTest : public testing::Test {
 public:
  ProfileSyncServiceStartupTest()
      : ui_thread_(ChromeThread::UI, &message_loop_) {}

  virtual ~ProfileSyncServiceStartupTest() {
    // The PSS has some deletes that are scheduled on the main thread
    // so we must delete the service and run the message loop.
    service_.reset();
    MessageLoop::current()->RunAllPending();
  }

  virtual void SetUp() {
    service_.reset(new TestProfileSyncService(&profile_, false));
    service_->AddObserver(&observer_);
  }

  virtual void TearDown() {
    service_->RemoveObserver(&observer_);
  }

 protected:
  DataTypeControllerMock* MakeBookmarkDTC() {
    DataTypeControllerMock* dtc = new DataTypeControllerMock();
    EXPECT_CALL(*dtc, enabled()).WillRepeatedly(Return(true));
    EXPECT_CALL(*dtc, model_safe_group()).
        WillRepeatedly(Return(browser_sync::GROUP_UI));
    EXPECT_CALL(*dtc, type()).WillRepeatedly(Return(syncable::BOOKMARKS));
    return dtc;
  }

  DataTypeControllerMock* MakePreferenceDTC() {
    DataTypeControllerMock* dtc = new DataTypeControllerMock();
    EXPECT_CALL(*dtc, enabled()).WillRepeatedly(Return(true));
    EXPECT_CALL(*dtc, model_safe_group()).
        WillRepeatedly(Return(browser_sync::GROUP_UI));
    EXPECT_CALL(*dtc, type()).WillRepeatedly(Return(syncable::PREFERENCES));
    return dtc;
  }

  void SetStartStopExpectations(DataTypeControllerMock* mock_dtc) {
    EXPECT_CALL(*mock_dtc, Start(_, _)).
        WillOnce(InvokeCallback((DataTypeController::OK)));
    EXPECT_CALL(*mock_dtc, Stop()).Times(1);
    EXPECT_CALL(*mock_dtc, state()).
        WillOnce(Return(DataTypeController::RUNNING));
  }

  MessageLoop message_loop_;
  ChromeThread ui_thread_;
  TestingProfile profile_;
  scoped_ptr<TestProfileSyncService> service_;
  ProfileSyncServiceObserverMock observer_;
};

TEST_F(ProfileSyncServiceStartupTest, SKIP_MACOSX(StartFirstTime)) {
  DataTypeControllerMock* bookmark_dtc = MakeBookmarkDTC();
  EXPECT_CALL(*bookmark_dtc, Start(false, _)).Times(0);
  EXPECT_CALL(*bookmark_dtc, state()).
      WillRepeatedly(Return(DataTypeController::NOT_RUNNING));

  EXPECT_CALL(observer_, OnStateChanged()).Times(1);

  profile_.GetPrefs()->ClearPref(prefs::kSyncHasSetupCompleted);
  service_->RegisterDataTypeController(bookmark_dtc);
  // Should not actually start, rather just clean things up and wait
  // to be enabled.
  service_->Initialize();

  // Preferences should be back to defaults.
  EXPECT_EQ(0, profile_.GetPrefs()->GetInt64(prefs::kSyncLastSyncedTime));
  EXPECT_FALSE(profile_.GetPrefs()->GetBoolean(prefs::kSyncHasSetupCompleted));
  Mock::VerifyAndClearExpectations(&bookmark_dtc);

  // Then start things up.
  SetStartStopExpectations(bookmark_dtc);
  EXPECT_CALL(observer_, OnStateChanged()).Times(3);
  service_->EnableForUser();
}

TEST_F(ProfileSyncServiceStartupTest, SKIP_MACOSX(StartBookmarks)) {
  DataTypeControllerMock* bookmark_dtc = MakeBookmarkDTC();
  SetStartStopExpectations(bookmark_dtc);

  EXPECT_CALL(observer_, OnStateChanged()).Times(2);

  service_->RegisterDataTypeController(bookmark_dtc);
  service_->Initialize();
}

TEST_F(ProfileSyncServiceStartupTest, SKIP_MACOSX(StartBookmarksWithMerge)) {
  DataTypeControllerMock* bookmark_dtc = MakeBookmarkDTC();
  EXPECT_CALL(*bookmark_dtc, Start(false, _)).
      WillOnce(InvokeCallback((DataTypeController::NEEDS_MERGE)));
  EXPECT_CALL(*bookmark_dtc, Start(true, _)).
      WillOnce(InvokeCallback((DataTypeController::OK)));
  EXPECT_CALL(*bookmark_dtc, state()).
      WillOnce(Return(DataTypeController::NOT_RUNNING)).
      WillOnce(Return(DataTypeController::RUNNING));

  EXPECT_CALL(*bookmark_dtc, Stop()).Times(1);

  EXPECT_CALL(observer_, OnStateChanged()).Times(4);

  // Set kSyncHasSetupCompleted to false so the first attempt to start
  // the bookmark dtc is done with merge_allowed = false.  This has
  // the side effect of disabling sync, so we must explicitly call
  // EnableForUser.
  profile_.GetPrefs()->SetBoolean(prefs::kSyncHasSetupCompleted, false);
  service_->RegisterDataTypeController(bookmark_dtc);
  service_->Initialize();
  service_->EnableForUser();
  service_->OnUserAcceptedMergeAndSync();
}

TEST_F(ProfileSyncServiceStartupTest, SKIP_MACOSX(StartPreferences)) {
  DataTypeControllerMock* preference_dtc = MakePreferenceDTC();
  SetStartStopExpectations(preference_dtc);

  EXPECT_CALL(observer_, OnStateChanged()).Times(2);

  service_->RegisterDataTypeController(preference_dtc);
  service_->Initialize();
}

TEST_F(ProfileSyncServiceStartupTest,
       SKIP_MACOSX(StartBookmarksAndPreferences)) {
  DataTypeControllerMock* bookmark_dtc = MakeBookmarkDTC();
  SetStartStopExpectations(bookmark_dtc);

  DataTypeControllerMock* preference_dtc = MakePreferenceDTC();
  SetStartStopExpectations(preference_dtc);

  EXPECT_CALL(observer_, OnStateChanged()).Times(2);

  service_->RegisterDataTypeController(bookmark_dtc);
  service_->RegisterDataTypeController(preference_dtc);
  service_->Initialize();
}

TEST_F(ProfileSyncServiceStartupTest,
       SKIP_MACOSX(StartBookmarksAndPreferencesFail)) {
  DataTypeControllerMock* bookmark_dtc = MakeBookmarkDTC();
  EXPECT_CALL(*bookmark_dtc, Start(_, _)).
      WillOnce(InvokeCallback((DataTypeController::OK)));
  EXPECT_CALL(*bookmark_dtc, Stop()).Times(1);
  EXPECT_CALL(*bookmark_dtc, state()).
      WillOnce(Return(DataTypeController::RUNNING)).
      WillOnce(Return(DataTypeController::NOT_RUNNING));

  DataTypeControllerMock* preference_dtc = MakePreferenceDTC();
  EXPECT_CALL(*preference_dtc, Start(_, _)).
      WillOnce(InvokeCallback((DataTypeController::ASSOCIATION_FAILED)));
  EXPECT_CALL(*preference_dtc, state()).
      WillOnce(Return(DataTypeController::NOT_RUNNING)).
      WillOnce(Return(DataTypeController::NOT_RUNNING));

  EXPECT_CALL(observer_, OnStateChanged()).Times(2);

  service_->RegisterDataTypeController(bookmark_dtc);
  service_->RegisterDataTypeController(preference_dtc);
  service_->Initialize();
  EXPECT_TRUE(service_->unrecoverable_error_detected());
}

class ProfileSyncServiceStartupBootstrapTest :
    public ProfileSyncServiceStartupTest {
 public:
  ProfileSyncServiceStartupBootstrapTest() {}
  virtual ~ProfileSyncServiceStartupBootstrapTest() {}

  virtual void SetUp() {
    service_.reset(new TestProfileSyncService(&profile_, true));
    service_->AddObserver(&observer_);
  }
};

TEST_F(ProfileSyncServiceStartupBootstrapTest, SKIP_MACOSX(StartFirstTime)) {
  DataTypeControllerMock* bookmark_dtc = MakeBookmarkDTC();
  EXPECT_CALL(*bookmark_dtc, Start(_, _)).
      WillOnce(InvokeCallback((DataTypeController::OK)));
  EXPECT_CALL(*bookmark_dtc, Stop()).Times(1);
  EXPECT_CALL(*bookmark_dtc, state()).
      WillOnce(Return(DataTypeController::NOT_RUNNING)).
      WillOnce(Return(DataTypeController::RUNNING));
  EXPECT_CALL(observer_, OnStateChanged()).Times(3);

  profile_.GetPrefs()->ClearPref(prefs::kSyncHasSetupCompleted);
  service_->RegisterDataTypeController(bookmark_dtc);
  // Will start sync even though setup hasn't been completed (since
  // setup is bypassed when bootstrapping is enabled).
  service_->Initialize();
}

TEST_F(ProfileSyncServiceStartupBootstrapTest,
       SKIP_MACOSX(StartBookmarksWithMerge)) {
  DataTypeControllerMock* bookmark_dtc = MakeBookmarkDTC();
  EXPECT_CALL(*bookmark_dtc, Start(true, _)).
      WillOnce(InvokeCallback((DataTypeController::OK)));
  EXPECT_CALL(*bookmark_dtc, Stop()).Times(1);
  EXPECT_CALL(*bookmark_dtc, state()).
      WillOnce(Return(DataTypeController::NOT_RUNNING)).
      WillOnce(Return(DataTypeController::RUNNING));

  EXPECT_CALL(observer_, OnStateChanged()).Times(3);

  // Set kSyncHasSetupCompleted to false so the first attempt to start
  // the bookmark dtc is done with merge_allowed = false.  This has
  // the side effect of disabling sync, but this does not impede
  // startup when bootstrapping is enabled.
  profile_.GetPrefs()->SetBoolean(prefs::kSyncHasSetupCompleted, false);
  service_->RegisterDataTypeController(bookmark_dtc);
  service_->Initialize();
}
