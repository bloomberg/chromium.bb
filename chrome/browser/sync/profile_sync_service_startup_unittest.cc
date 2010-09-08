// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/net/gaia/token_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/sync/glue/data_type_manager.h"
#include "chrome/browser/sync/glue/data_type_manager_mock.h"
#include "chrome/browser/sync/profile_sync_factory_mock.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/browser/sync/test_profile_sync_service.h"
#include "chrome/common/net/gaia/gaia_auth_consumer.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/testing_profile.h"
#include "testing/gmock/include/gmock/gmock.h"

using browser_sync::DataTypeManager;
using browser_sync::DataTypeManagerMock;
using testing::_;
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
    service_.reset(new TestProfileSyncService(&factory_, &profile_,
                                              false, true, NULL));
    service_->AddObserver(&observer_);
    service_->set_num_expected_resumes(0);
    service_->set_num_expected_pauses(0);
    service_->set_synchronous_sync_configuration();
  }

  virtual void TearDown() {
    service_->RemoveObserver(&observer_);
  }

 protected:
  DataTypeManagerMock* SetUpDataTypeManager() {
    DataTypeManagerMock* data_type_manager = new DataTypeManagerMock();
    EXPECT_CALL(factory_, CreateDataTypeManager(_, _)).
        WillOnce(Return(data_type_manager));
    return data_type_manager;
  }

  MessageLoop message_loop_;
  ChromeThread ui_thread_;
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
  EXPECT_CALL(observer_, OnStateChanged()).Times(1);
  service_->Initialize();

  // Preferences should be back to defaults.
  EXPECT_EQ(0, profile_.GetPrefs()->GetInt64(prefs::kSyncLastSyncedTime));
  EXPECT_FALSE(profile_.GetPrefs()->GetBoolean(prefs::kSyncHasSetupCompleted));
  Mock::VerifyAndClearExpectations(data_type_manager);

  // Then start things up.
  EXPECT_CALL(*data_type_manager, Configure(_)).Times(1);
  EXPECT_CALL(*data_type_manager, state()).
      WillOnce(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*data_type_manager, Stop()).Times(1);
  EXPECT_CALL(observer_, OnStateChanged()).Times(4);
  service_->EnableForUser(NULL);
  syncable::ModelTypeSet set;
  set.insert(syncable::BOOKMARKS);
  service_->OnUserChoseDatatypes(false, set);
}

TEST_F(ProfileSyncServiceStartupTest, SKIP_MACOSX(StartNormal)) {
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManager();
  EXPECT_CALL(*data_type_manager, Configure(_)).Times(1);
  EXPECT_CALL(*data_type_manager, state()).
      WillOnce(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*data_type_manager, Stop()).Times(1);

  EXPECT_CALL(observer_, OnStateChanged()).Times(3);

  service_->Initialize();
}

TEST_F(ProfileSyncServiceStartupTest, SKIP_MACOSX(ManagedStartup)) {
  // Disable sync through policy.
  profile_.GetPrefs()->SetBoolean(prefs::kSyncManaged, true);

  EXPECT_CALL(factory_, CreateDataTypeManager(_, _)).Times(0);
  EXPECT_CALL(observer_, OnStateChanged()).Times(1);

  // Service should not be started by Initialize() since it's managed.
  service_->Initialize();
}

TEST_F(ProfileSyncServiceStartupTest, SKIP_MACOSX(SwitchManaged)) {
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManager();
  EXPECT_CALL(*data_type_manager, Configure(_)).Times(1);
  EXPECT_CALL(observer_, OnStateChanged()).Times(3);

  service_->Initialize();

  // The service should stop when switching to managed mode.
  Mock::VerifyAndClearExpectations(data_type_manager);
  EXPECT_CALL(*data_type_manager, state()).
      WillOnce(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*data_type_manager, Stop()).Times(1);
  EXPECT_CALL(observer_, OnStateChanged()).Times(2);
  profile_.GetPrefs()->SetBoolean(prefs::kSyncManaged, true);

  // When switching back to unmanaged, the state should change, but the service
  // should not start up automatically (kSyncSetupCompleted will be false).
  Mock::VerifyAndClearExpectations(data_type_manager);
  EXPECT_CALL(factory_, CreateDataTypeManager(_, _)).Times(0);
  EXPECT_CALL(observer_, OnStateChanged()).Times(1);
  profile_.GetPrefs()->ClearPref(prefs::kSyncManaged);
}

TEST_F(ProfileSyncServiceStartupTest, SKIP_MACOSX(StartFailure)) {
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManager();
  DataTypeManager::ConfigureResult result =
      DataTypeManager::ASSOCIATION_FAILED;
  EXPECT_CALL(*data_type_manager, Configure(_)).
      WillOnce(DoAll(Notify(NotificationType::SYNC_CONFIGURE_START),
                     NotifyWithResult(NotificationType::SYNC_CONFIGURE_DONE,
                                      &result)));
  EXPECT_CALL(*data_type_manager, Stop()).Times(1);
  EXPECT_CALL(*data_type_manager, state()).
      WillOnce(Return(DataTypeManager::STOPPED));

  EXPECT_CALL(observer_, OnStateChanged()).Times(3);

  service_->Initialize();
  EXPECT_TRUE(service_->unrecoverable_error_detected());
}

class ProfileSyncServiceStartupBootstrapTest
    : public ProfileSyncServiceStartupTest {
 public:
  ProfileSyncServiceStartupBootstrapTest() {}
  virtual ~ProfileSyncServiceStartupBootstrapTest() {}

  virtual void SetUp() {
    service_.reset(new TestProfileSyncService(&factory_, &profile_,
                                              true, true, NULL));
    service_->AddObserver(&observer_);
    service_->set_num_expected_resumes(0);
    service_->set_num_expected_pauses(0);
    service_->set_synchronous_sync_configuration();
  }
};

TEST_F(ProfileSyncServiceStartupBootstrapTest, SKIP_MACOSX(StartFirstTime)) {
  DataTypeManagerMock* data_type_manager = SetUpDataTypeManager();
  EXPECT_CALL(*data_type_manager, Configure(_)).Times(1);
  EXPECT_CALL(*data_type_manager, state()).
      WillOnce(Return(DataTypeManager::CONFIGURED));
  EXPECT_CALL(*data_type_manager, Stop()).Times(1);
  EXPECT_CALL(observer_, OnStateChanged()).Times(4);

  profile_.GetPrefs()->ClearPref(prefs::kSyncHasSetupCompleted);

  // Pretend the login screen worked.
  GaiaAuthConsumer::ClientLoginResult result;
  result.sid = "sid";
  result.lsid = "lsid";
  profile_.GetTokenService()->Initialize("test",
                                         &profile_);
  profile_.GetTokenService()->UpdateCredentials(result);

  // Will start sync even though setup hasn't been completed (since
  // setup is bypassed when bootstrapping is enabled).
  service_->Initialize();
}
