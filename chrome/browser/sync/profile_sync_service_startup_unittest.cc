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
#include "chrome/browser/sync/profile_sync_components_factory_mock.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/browser/sync/signin_manager.h"
#include "chrome/browser/sync/test_profile_sync_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/net/gaia/gaia_auth_consumer.h"
#include "chrome/common/net/gaia/gaia_constants.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_profile.h"
#include "content/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"

using browser_sync::DataTypeManager;
using browser_sync::DataTypeManagerMock;
using content::BrowserThread;
using testing::_;
using testing::AnyNumber;
using testing::DoAll;
using testing::InvokeArgument;
using testing::Mock;
using testing::Return;

// TODO(chron): Test not using cros_user flag and use signin_
class ProfileSyncServiceStartupTest : public testing::Test {
 public:
  ProfileSyncServiceStartupTest()
      : ui_thread_(BrowserThread::UI, &ui_loop_),
        io_thread_(BrowserThread::IO),
        profile_(new TestingProfile) {}

  virtual ~ProfileSyncServiceStartupTest() {
  }

  virtual void SetUp() {
    io_thread_.StartIOThread();
    profile_->CreateRequestContext();
    CreateSyncService();
    service_->AddObserver(&observer_);
    service_->set_synchronous_sync_configuration();
  }

  virtual void TearDown() {
    service_->RemoveObserver(&observer_);
    service_.reset();
    profile_.reset();

    // Pump messages posted by the sync core thread (which may end up
    // posting on the IO thread).
    ui_loop_.RunAllPending();
    io_thread_.Stop();
    ui_loop_.RunAllPending();
  }

 protected:
  // Overridden below by ProfileSyncServiceStartupCrosTest.
  virtual void CreateSyncService() {
    service_.reset(new TestProfileSyncService(
        &factory_, profile_.get(), "", true, base::Closure()));
  }

  DataTypeManagerMock* SetUpDataTypeManager() {
    DataTypeManagerMock* data_type_manager = new DataTypeManagerMock();
    EXPECT_CALL(factory_, CreateDataTypeManager(_, _)).
        WillOnce(Return(data_type_manager));
    return data_type_manager;
  }

  MessageLoop ui_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;
  scoped_ptr<TestingProfile> profile_;
  ProfileSyncComponentsFactoryMock factory_;
  scoped_ptr<TestProfileSyncService> service_;
  ProfileSyncServiceObserverMock observer_;
};

class ProfileSyncServiceStartupCrosTest : public ProfileSyncServiceStartupTest {
 protected:
  virtual void CreateSyncService() {
    service_.reset(new TestProfileSyncService(
        &factory_, profile_.get(), "test_user", true, base::Closure()));
  }
};

TEST_F(ProfileSyncServiceStartupTest, StartFirstTime) {
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManager();
  EXPECT_CALL(*data_type_manager, Configure(_, _)).Times(0);

  // We've never completed startup.
  profile_->GetPrefs()->ClearPref(prefs::kSyncHasSetupCompleted);

  // Should not actually start, rather just clean things up and wait
  // to be enabled.
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());
  service_->Initialize();

  // Preferences should be back to defaults.
  EXPECT_EQ(0, profile_->GetPrefs()->GetInt64(prefs::kSyncLastSyncedTime));
  EXPECT_FALSE(profile_->GetPrefs()->GetBoolean(prefs::kSyncHasSetupCompleted));
  Mock::VerifyAndClearExpectations(data_type_manager);

  // Then start things up.
  EXPECT_CALL(*data_type_manager, Configure(_, _)).Times(1);
  EXPECT_CALL(*data_type_manager, state()).
      WillOnce(Return(DataTypeManager::CONFIGURED)).
      WillOnce(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*data_type_manager, Stop()).Times(1);
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());

  // Create some tokens in the token service; the service will startup when
  // it is notified that tokens are available.
  service_->OnUserSubmittedAuth("test_user", "", "", "");
  profile_->GetTokenService()->IssueAuthTokenForTest(
      GaiaConstants::kSyncService, "sync_token");

  syncable::ModelTypeSet set;
  set.insert(syncable::BOOKMARKS);
  service_->OnUserChoseDatatypes(false, set);
  EXPECT_TRUE(service_->ShouldPushChanges());
}

TEST_F(ProfileSyncServiceStartupCrosTest, StartFirstTime) {
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManager();
  profile_->GetPrefs()->ClearPref(prefs::kSyncHasSetupCompleted);
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  EXPECT_CALL(*data_type_manager, state()).
      WillRepeatedly(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*data_type_manager, Stop()).Times(1);
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());

  profile_->GetTokenService()->IssueAuthTokenForTest(
      GaiaConstants::kSyncService, "sync_token");
  service_->Initialize();
  EXPECT_TRUE(service_->ShouldPushChanges());
}

TEST_F(ProfileSyncServiceStartupTest, StartNormal) {
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManager();
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  EXPECT_CALL(*data_type_manager, state()).
      WillRepeatedly(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*data_type_manager, Stop()).Times(1);

  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());

  // Pre load the tokens
  profile_->GetTokenService()->IssueAuthTokenForTest(
      GaiaConstants::kSyncService, "sync_token");
  profile_->GetPrefs()->SetString(prefs::kGoogleServicesUsername, "test_user");
  service_->Initialize();
}

TEST_F(ProfileSyncServiceStartupTest, ManagedStartup) {
  // Disable sync through policy.
  profile_->GetPrefs()->SetBoolean(prefs::kSyncManaged, true);

  EXPECT_CALL(factory_, CreateDataTypeManager(_, _)).Times(0);
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());

  // Service should not be started by Initialize() since it's managed.
  profile_->GetTokenService()->IssueAuthTokenForTest(
      GaiaConstants::kSyncService, "sync_token");
  profile_->GetPrefs()->SetString(prefs::kGoogleServicesUsername, "test_user");
  service_->Initialize();
}

TEST_F(ProfileSyncServiceStartupTest, SwitchManaged) {
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManager();
  EXPECT_CALL(*data_type_manager, Configure(_, _));
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());

  profile_->GetTokenService()->IssueAuthTokenForTest(
      GaiaConstants::kSyncService, "sync_token");
  profile_->GetPrefs()->SetString(prefs::kGoogleServicesUsername, "test_user");
  service_->Initialize();

  // The service should stop when switching to managed mode.
  Mock::VerifyAndClearExpectations(data_type_manager);
  EXPECT_CALL(*data_type_manager, state()).
      WillOnce(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*data_type_manager, Stop()).Times(1);
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());
  profile_->GetPrefs()->SetBoolean(prefs::kSyncManaged, true);

  // When switching back to unmanaged, the state should change, but the service
  // should not start up automatically (kSyncSetupCompleted will be false).
  Mock::VerifyAndClearExpectations(data_type_manager);
  EXPECT_CALL(factory_, CreateDataTypeManager(_, _)).Times(0);
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());
  profile_->GetPrefs()->ClearPref(prefs::kSyncManaged);
}

TEST_F(ProfileSyncServiceStartupTest, ClearServerData) {
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManager();
  EXPECT_CALL(*data_type_manager, Configure(_, _)).Times(1);
  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());

  profile_->GetTokenService()->IssueAuthTokenForTest(
      GaiaConstants::kSyncService, "sync_token");
  profile_->GetPrefs()->SetString(prefs::kGoogleServicesUsername, "test_user");
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

TEST_F(ProfileSyncServiceStartupTest, StartFailure) {
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManager();
  DataTypeManager::ConfigureStatus status = DataTypeManager::ABORTED;
  SyncError error(FROM_HERE, "Association failed.", syncable::BOOKMARKS);
  std::list<SyncError> errors;
  errors.push_back(error);
  browser_sync::DataTypeManager::ConfigureResult result(
      status,
      syncable::ModelTypeSet(),
      errors);
  EXPECT_CALL(*data_type_manager, Configure(_, _)).
      WillRepeatedly(
          DoAll(
              NotifyFromDataTypeManager(data_type_manager,
                  static_cast<int>(chrome::NOTIFICATION_SYNC_CONFIGURE_START)),
              NotifyFromDataTypeManagerWithResult(data_type_manager,
                  static_cast<int>(chrome::NOTIFICATION_SYNC_CONFIGURE_DONE),
                  &result)));
  EXPECT_CALL(*data_type_manager, state()).
      WillOnce(Return(DataTypeManager::STOPPED));

  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());

  profile_->GetTokenService()->IssueAuthTokenForTest(
      GaiaConstants::kSyncService, "sync_token");
  profile_->GetPrefs()->SetString(prefs::kGoogleServicesUsername, "test_user");
  service_->Initialize();
  EXPECT_TRUE(service_->unrecoverable_error_detected());
}

TEST_F(ProfileSyncServiceStartupTest, StartDownloadFailed) {
  profile_->GetPrefs()->ClearPref(prefs::kSyncHasSetupCompleted);

  EXPECT_CALL(observer_, OnStateChanged()).Times(AnyNumber());

  // Preload the tokens.
  profile_->GetTokenService()->IssueAuthTokenForTest(
      GaiaConstants::kSyncService, "sync_token");
  service_->fail_initial_download();

  service_->Initialize();
  EXPECT_FALSE(service_->sync_initialized());
  EXPECT_FALSE(service_->GetBackendForTest());
}
