// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/sync/glue/data_type_manager.h"
#include "chrome/browser/sync/glue/data_type_manager_mock.h"
#include "chrome/browser/sync/profile_sync_factory_mock.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/browser/sync/test_profile_sync_service.h"
#include "chrome/common/net/gaia/gaia_auth_consumer.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/testing_profile.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_type.h"
#include "testing/gmock/include/gmock/gmock.h"

using browser_sync::DataTypeManager;
using browser_sync::DataTypeManagerMock;
using testing::_;
using testing::AnyNumber;
using testing::DoAll;
using testing::InvokeArgument;
using testing::Mock;
using testing::Return;

ACTION_P(InvokeCallback, callback_result) {
  arg0->Run(callback_result);
  delete arg0;
}

// TODO(skrul) This test fails on the mac. See http://crbug.com/33443
#if defined(OS_MACOSX)
#define SKIP_MACOSX(test) DISABLED_##test
#else
#define SKIP_MACOSX(test) test
#endif

// TODO(chron): Test not using cros_user flag and use signin_
class ProfileSyncServiceStartupTest : public testing::Test {
 public:
  ProfileSyncServiceStartupTest()
      : ui_thread_(BrowserThread::UI, &message_loop_) {}

  virtual ~ProfileSyncServiceStartupTest() {
    // The PSS has some deletes that are scheduled on the main thread
    // so we must delete the service and run the message loop.
    service_.reset();
    MessageLoop::current()->RunAllPending();
  }

  virtual void SetUp() {
    profile_.CreateRequestContext();
    service_.reset(new TestProfileSyncService(&factory_, &profile_,
                                              "test", true, NULL));
    service_->AddObserver(&observer_);
    service_->set_synchronous_sync_configuration();
  }

  virtual void TearDown() {
    service_->RemoveObserver(&observer_);
    {
      // The request context gets deleted on the I/O thread. To prevent a leak
      // supply one here.
      BrowserThread io_thread(BrowserThread::IO, MessageLoop::current());
      profile_.ResetRequestContext();
    }
    MessageLoop::current()->RunAllPending();
  }

 protected:
  DataTypeManagerMock* SetUpDataTypeManager() {
    DataTypeManagerMock* data_type_manager = new DataTypeManagerMock();
    EXPECT_CALL(factory_, CreateDataTypeManager(_, _)).
        WillOnce(Return(data_type_manager));
    return data_type_manager;
  }

  MessageLoop message_loop_;
  BrowserThread ui_thread_;
  TestingProfile profile_;
  ProfileSyncFactoryMock factory_;
  scoped_ptr<TestProfileSyncService> service_;
  ProfileSyncServiceObserverMock observer_;
};

TEST_F(ProfileSyncServiceStartupTest, SKIP_MACOSX(StartFirstTime)) {
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManager();
  EXPECT_CALL(*data_type_manager, Configure(_)).Times(0);

  // We've never completed startup.
  profile_.GetPrefs()->ClearPref(prefs::kSyncHasSetupCompleted);

  // Should not actually start, rather just clean things up and wait
  // to be enabled.
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());
  service_->Initialize();

  // Preferences should be back to defaults.
  EXPECT_EQ(0, profile_.GetPrefs()->GetInt64(prefs::kSyncLastSyncedTime));
  EXPECT_FALSE(profile_.GetPrefs()->GetBoolean(prefs::kSyncHasSetupCompleted));
  Mock::VerifyAndClearExpectations(data_type_manager);

  // Then start things up.
  EXPECT_CALL(*data_type_manager, Configure(_)).Times(3);
  EXPECT_CALL(*data_type_manager, state()).
      WillOnce(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*data_type_manager, Stop()).Times(1);
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());

  // Create some tokens in the token service; the service will startup when
  // it is notified that tokens are available.
  profile_.GetTokenService()->IssueAuthTokenForTest(
      GaiaConstants::kSyncService, "sync_token");

  syncable::ModelTypeSet set;
  set.insert(syncable::BOOKMARKS);
  service_->OnUserChoseDatatypes(false, set);
}

TEST_F(ProfileSyncServiceStartupTest, SKIP_MACOSX(StartNormal)) {
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManager();
  EXPECT_CALL(*data_type_manager, Configure(_)).Times(2);
  EXPECT_CALL(*data_type_manager, state()).
      WillOnce(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*data_type_manager, Stop()).Times(1);

  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());

  // Pre load the tokens
  profile_.GetTokenService()->IssueAuthTokenForTest(
      GaiaConstants::kSyncService, "sync_token");
  service_->Initialize();
}

TEST_F(ProfileSyncServiceStartupTest, SKIP_MACOSX(ManagedStartup)) {
  // Disable sync through policy.
  profile_.GetPrefs()->SetBoolean(prefs::kSyncManaged, true);

  EXPECT_CALL(factory_, CreateDataTypeManager(_, _)).Times(0);
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());

  // Service should not be started by Initialize() since it's managed.
  profile_.GetTokenService()->IssueAuthTokenForTest(
      GaiaConstants::kSyncService, "sync_token");
  service_->Initialize();
}

TEST_F(ProfileSyncServiceStartupTest, SKIP_MACOSX(SwitchManaged)) {
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManager();
  EXPECT_CALL(*data_type_manager, Configure(_)).Times(2);
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());

  profile_.GetTokenService()->IssueAuthTokenForTest(
      GaiaConstants::kSyncService, "sync_token");
  service_->Initialize();

  // The service should stop when switching to managed mode.
  Mock::VerifyAndClearExpectations(data_type_manager);
  EXPECT_CALL(*data_type_manager, state()).
      WillOnce(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*data_type_manager, Stop()).Times(1);
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());
  profile_.GetPrefs()->SetBoolean(prefs::kSyncManaged, true);

  // When switching back to unmanaged, the state should change, but the service
  // should not start up automatically (kSyncSetupCompleted will be false).
  Mock::VerifyAndClearExpectations(data_type_manager);
  EXPECT_CALL(factory_, CreateDataTypeManager(_, _)).Times(0);
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());
  profile_.GetPrefs()->ClearPref(prefs::kSyncManaged);
}

TEST_F(ProfileSyncServiceStartupTest, ClearServerData) {
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManager();
  EXPECT_CALL(*data_type_manager, Configure(_)).Times(2);
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());

  profile_.GetTokenService()->IssueAuthTokenForTest(
      GaiaConstants::kSyncService, "sync_token");
  service_->Initialize();
  Mock::VerifyAndClearExpectations(data_type_manager);

  // Success can overwrite failure, failure cannot overwrite success.  We want
  // this behavior because once clear has succeeded, sync gets disabled, and
  // we don't care about any subsequent failures (e.g. timeouts)
  service_->ResetClearServerDataState();
  EXPECT_TRUE(ProfileSyncService::CLEAR_NOT_STARTED ==
      service_->GetClearServerDataState());

  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());
  service_->OnClearServerDataFailed();
  EXPECT_TRUE(ProfileSyncService::CLEAR_FAILED ==
      service_->GetClearServerDataState());

  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());
  service_->OnClearServerDataSucceeded();
  EXPECT_TRUE(ProfileSyncService::CLEAR_SUCCEEDED ==
      service_->GetClearServerDataState());

  service_->OnClearServerDataFailed();
  EXPECT_TRUE(ProfileSyncService::CLEAR_SUCCEEDED ==
      service_->GetClearServerDataState());

  // Now test the timeout states
  service_->ResetClearServerDataState();
  EXPECT_TRUE(ProfileSyncService::CLEAR_NOT_STARTED ==
      service_->GetClearServerDataState());

  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());
  service_->OnClearServerDataTimeout();
  EXPECT_TRUE(ProfileSyncService::CLEAR_FAILED ==
      service_->GetClearServerDataState());

  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());
  service_->OnClearServerDataSucceeded();
  EXPECT_TRUE(ProfileSyncService::CLEAR_SUCCEEDED ==
      service_->GetClearServerDataState());

  service_->OnClearServerDataFailed();
  EXPECT_TRUE(ProfileSyncService::CLEAR_SUCCEEDED ==
      service_->GetClearServerDataState());

  // Test the pending state, doesn't matter what state
  // the back end syncmgr returns
  EXPECT_CALL(*data_type_manager, state()).
    WillOnce(Return(DataTypeManager::STOPPED));
  service_->ResetClearServerDataState();
  service_->ClearServerData();
  EXPECT_TRUE(ProfileSyncService::CLEAR_CLEARING ==
      service_->GetClearServerDataState());

  // Stop the timer and reset the state
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());
  service_->OnClearServerDataSucceeded();
  service_->ResetClearServerDataState();
}

TEST_F(ProfileSyncServiceStartupTest, SKIP_MACOSX(StartFailure)) {
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManager();
  DataTypeManager::ConfigureResult configure_result =
      DataTypeManager::ASSOCIATION_FAILED;
  browser_sync::DataTypeManager::ConfigureResultWithErrorLocation result(
      configure_result, FROM_HERE, syncable::ModelTypeSet());
  EXPECT_CALL(*data_type_manager, Configure(_)).
      WillRepeatedly(DoAll(NotifyFromDataTypeManager(data_type_manager,
                         NotificationType::SYNC_CONFIGURE_START),
                     NotifyFromDataTypeManagerWithResult(data_type_manager,
                         NotificationType::SYNC_CONFIGURE_DONE,
                         &result)));
  EXPECT_CALL(*data_type_manager, state()).
      WillOnce(Return(DataTypeManager::STOPPED));

  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());

  profile_.GetTokenService()->IssueAuthTokenForTest(
      GaiaConstants::kSyncService, "sync_token");
  service_->Initialize();
  EXPECT_TRUE(service_->unrecoverable_error_detected());
}
