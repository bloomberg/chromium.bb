// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/data_type_manager_impl.h"

#include <set>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/glue/data_type_controller_mock.h"
#include "chrome/browser/sync/glue/data_type_manager_mock.h"
#include "chrome/browser/sync/glue/sync_backend_host_mock.h"
#include "chrome/browser/sync/internal_api/configure_reason.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/test/notification_observer_mock.h"
#include "content/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using browser_sync::DataTypeManager;
using browser_sync::DataTypeManagerImpl;
using browser_sync::DataTypeController;
using browser_sync::DataTypeControllerMock;
using browser_sync::SyncBackendHostMock;
using content::BrowserThread;
using testing::_;
using testing::AtLeast;
using testing::DoAll;
using testing::DoDefault;
using testing::InSequence;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::InvokeWithoutArgs;
using testing::Mock;
using testing::NiceMock;
using testing::Property;
using testing::Pointee;
using testing::Return;
using testing::SaveArg;

DataTypeManager::ConfigureStatus GetStatus(
    const content::NotificationDetails& details) {
  const DataTypeManager::ConfigureResult* result =
      content::Details<const DataTypeManager::ConfigureResult>(
      details).ptr();
  return result->status;
}

void DoConfigureDataTypes(
    const syncable::ModelTypeSet& types_to_add,
    const syncable::ModelTypeSet& types_to_remove,
    sync_api::ConfigureReason reason,
    base::Callback<void(const syncable::ModelTypeSet&)> ready_task,
    bool enable_nigori) {
  ready_task.Run(syncable::ModelTypeSet());
}

void QuitMessageLoop() {
  MessageLoop::current()->Quit();
}

class DataTypeManagerImplTest : public testing::Test {
 public:
  DataTypeManagerImplTest()
      : ui_thread_(BrowserThread::UI, &message_loop_) {}

  virtual ~DataTypeManagerImplTest() {
  }

 protected:
  virtual void SetUp() {
    registrar_.Add(&observer_,
                   chrome::NOTIFICATION_SYNC_CONFIGURE_START,
                   content::NotificationService::AllSources());
    registrar_.Add(&observer_,
                   chrome::NOTIFICATION_SYNC_CONFIGURE_DONE,
                   content::NotificationService::AllSources());
  }

  DataTypeControllerMock* MakeBookmarkDTC() {
    DataTypeControllerMock* dtc = new DataTypeControllerMock();
    EXPECT_CALL(*dtc, enabled()).WillRepeatedly(Return(true));
    EXPECT_CALL(*dtc, type()).WillRepeatedly(Return(syncable::BOOKMARKS));
    EXPECT_CALL(*dtc, name()).WillRepeatedly(Return("bookmark"));
    return dtc;
  }

  DataTypeControllerMock* MakePreferenceDTC() {
    DataTypeControllerMock* dtc = new DataTypeControllerMock();
    EXPECT_CALL(*dtc, enabled()).WillRepeatedly(Return(true));
    EXPECT_CALL(*dtc, type()).WillRepeatedly(Return(syncable::PREFERENCES));
    EXPECT_CALL(*dtc, name()).WillRepeatedly(Return("preference"));
    return dtc;
  }

  DataTypeControllerMock* MakePasswordDTC() {
    DataTypeControllerMock* dtc = new DataTypeControllerMock();
    SetPasswordDTCExpectations(dtc);
    return dtc;
  }

  void SetPasswordDTCExpectations(DataTypeControllerMock* dtc) {
    EXPECT_CALL(*dtc, enabled()).WillRepeatedly(Return(true));
    EXPECT_CALL(*dtc, type()).WillRepeatedly(Return(syncable::PASSWORDS));
    EXPECT_CALL(*dtc, name()).WillRepeatedly(Return("passwords"));
  }

  void SetStartStopExpectations(DataTypeControllerMock* mock_dtc) {
    InSequence seq;
    EXPECT_CALL(*mock_dtc, state()).
        WillRepeatedly(Return(DataTypeController::NOT_RUNNING));
    EXPECT_CALL(*mock_dtc, Start(_)).
        WillOnce(InvokeCallback(syncable::BOOKMARKS, DataTypeController::OK));
    EXPECT_CALL(*mock_dtc, state()).
        WillRepeatedly(Return(DataTypeController::RUNNING));
    EXPECT_CALL(*mock_dtc, Stop()).Times(1);
    EXPECT_CALL(*mock_dtc, state()).
        WillRepeatedly(Return(DataTypeController::NOT_RUNNING));
  }

  void SetBusyStartStopExpectations(DataTypeControllerMock* mock_dtc,
                                    DataTypeController::State busy_state) {
    InSequence seq;
    EXPECT_CALL(*mock_dtc, state()).
        WillRepeatedly(Return(DataTypeController::NOT_RUNNING));
    EXPECT_CALL(*mock_dtc, Start(_)).
        WillOnce(InvokeCallback(syncable::BOOKMARKS, DataTypeController::OK));
    EXPECT_CALL(*mock_dtc, state()).
        WillRepeatedly(Return(busy_state));
    EXPECT_CALL(*mock_dtc, Stop()).Times(1);
    EXPECT_CALL(*mock_dtc, state()).
        WillRepeatedly(Return(DataTypeController::NOT_RUNNING));
  }

  void SetNotUsedExpectations(DataTypeControllerMock* mock_dtc) {
    EXPECT_CALL(*mock_dtc, Start(_)).Times(0);
    EXPECT_CALL(*mock_dtc, Stop()).Times(0);
    EXPECT_CALL(*mock_dtc, state()).
        WillRepeatedly(Return(DataTypeController::NOT_RUNNING));
  }

  void SetConfigureStartExpectation() {
    EXPECT_CALL(
        observer_,
        Observe(int(chrome::NOTIFICATION_SYNC_CONFIGURE_START),
                _, _));
  }


  void SetConfigureDoneExpectation(DataTypeManager::ConfigureStatus status) {
    EXPECT_CALL(
        observer_,
        Observe(int(chrome::NOTIFICATION_SYNC_CONFIGURE_DONE), _,
        ::testing::ResultOf(&GetStatus, status)));
  }

  void Configure(DataTypeManagerImpl* dtm,
                 const DataTypeManager::TypeSet& desired_types,
                 sync_api::ConfigureReason reason,
                 bool enable_nigori) {
    if (enable_nigori) {
      dtm->Configure(desired_types, reason);
    } else {
      dtm->ConfigureWithoutNigori(desired_types, reason);
    }
  }

  void RunConfigureOneTest(bool enable_nigori) {
    DataTypeControllerMock* bookmark_dtc = MakeBookmarkDTC();
    SetStartStopExpectations(bookmark_dtc);
    controllers_[syncable::BOOKMARKS] = bookmark_dtc;
    EXPECT_CALL(backend_,
                ConfigureDataTypes(_, _, _, _, enable_nigori)).Times(1);
    DataTypeManagerImpl dtm(&backend_, &controllers_);
    types_.insert(syncable::BOOKMARKS);
    SetConfigureStartExpectation();
    SetConfigureDoneExpectation(DataTypeManager::OK);
    Configure(&dtm, types_, sync_api::CONFIGURE_REASON_RECONFIGURATION,
              enable_nigori);
    EXPECT_EQ(DataTypeManager::CONFIGURED, dtm.state());
    dtm.Stop();
    EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
  }

  void RunConfigureWhileOneInFlightTest(bool enable_nigori) {
    DataTypeControllerMock* bookmark_dtc = MakeBookmarkDTC();
    // Save the callback here so we can interrupt startup.
    DataTypeController::StartCallback* callback;
    {
      InSequence seq;
      EXPECT_CALL(*bookmark_dtc, state()).
          WillRepeatedly(Return(DataTypeController::NOT_RUNNING));
      EXPECT_CALL(*bookmark_dtc, Start(_)).
          WillOnce(SaveArg<0>(&callback));
      EXPECT_CALL(*bookmark_dtc, state()).
          WillRepeatedly(Return(DataTypeController::RUNNING));
      EXPECT_CALL(*bookmark_dtc, Stop()).Times(1);
      EXPECT_CALL(*bookmark_dtc, state()).
          WillRepeatedly(Return(DataTypeController::NOT_RUNNING));
    }
    controllers_[syncable::BOOKMARKS] = bookmark_dtc;

    DataTypeControllerMock* preference_dtc = MakePreferenceDTC();
    SetStartStopExpectations(preference_dtc);
    controllers_[syncable::PREFERENCES] = preference_dtc;

    DataTypeManagerImpl dtm(&backend_, &controllers_);
    EXPECT_CALL(backend_, ConfigureDataTypes(_, _, _, _, enable_nigori))
        .WillOnce(Invoke(DoConfigureDataTypes))
        .WillOnce(DoAll(Invoke(DoConfigureDataTypes),
                        InvokeWithoutArgs(QuitMessageLoop)));

    types_.insert(syncable::BOOKMARKS);

    SetConfigureStartExpectation();
    SetConfigureDoneExpectation(DataTypeManager::OK);
    Configure(&dtm, types_, sync_api::CONFIGURE_REASON_RECONFIGURATION,
              enable_nigori);

    // At this point, the bookmarks dtc should be in flight.  Add
    // preferences and continue starting bookmarks.
    types_.insert(syncable::PREFERENCES);
    Configure(&dtm, types_, sync_api::CONFIGURE_REASON_RECONFIGURATION,
              enable_nigori);
    callback->Run(DataTypeController::OK, SyncError());
    delete callback;

    MessageLoop::current()->Run();

    EXPECT_EQ(DataTypeManager::CONFIGURED, dtm.state());

    dtm.Stop();
    EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
  }

  void RunConfigureWhileDownloadPendingTest(
      bool enable_nigori,
      const syncable::ModelTypeSet& first_configure_result) {
    DataTypeControllerMock* bookmark_dtc = MakeBookmarkDTC();
    SetStartStopExpectations(bookmark_dtc);
    controllers_[syncable::BOOKMARKS] = bookmark_dtc;

    DataTypeControllerMock* preference_dtc = MakePreferenceDTC();
    SetStartStopExpectations(preference_dtc);
    controllers_[syncable::PREFERENCES] = preference_dtc;

    DataTypeManagerImpl dtm(&backend_, &controllers_);
    SetConfigureStartExpectation();
    SetConfigureDoneExpectation(DataTypeManager::OK);
    base::Callback<void(const syncable::ModelTypeSet&)> task;
    // Grab the task the first time this is called so we can configure
    // before it is finished.
    EXPECT_CALL(backend_, ConfigureDataTypes(_, _, _, _, enable_nigori)).
        WillOnce(SaveArg<3>(&task)).
        WillOnce(DoDefault());

    types_.insert(syncable::BOOKMARKS);
    Configure(&dtm, types_, sync_api::CONFIGURE_REASON_RECONFIGURATION,
              enable_nigori);
    // Configure should stop in the DOWNLOAD_PENDING state because we
    // are waiting for the download ready task to be run.
    EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm.state());

    types_.insert(syncable::PREFERENCES);
    Configure(&dtm, types_, sync_api::CONFIGURE_REASON_RECONFIGURATION,
              enable_nigori);

    // Running the task will queue a restart task to the message loop, and
    // eventually get us configured.
    task.Run(first_configure_result);
    MessageLoop::current()->RunAllPending();
    EXPECT_EQ(DataTypeManager::CONFIGURED, dtm.state());

    dtm.Stop();
    EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
  }

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  DataTypeController::TypeMap controllers_;
  NiceMock<SyncBackendHostMock> backend_;
  content::NotificationObserverMock observer_;
  content::NotificationRegistrar registrar_;
  std::set<syncable::ModelType> types_;
};

TEST_F(DataTypeManagerImplTest, NoControllers) {
  DataTypeManagerImpl dtm(&backend_, &controllers_);
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK);
  dtm.Configure(types_, sync_api::CONFIGURE_REASON_RECONFIGURATION);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm.state());
  dtm.Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}

TEST_F(DataTypeManagerImplTest, ConfigureOne) {
  RunConfigureOneTest(true /* enable_nigori */);
}

TEST_F(DataTypeManagerImplTest, ConfigureOneWithoutNigori) {
  RunConfigureOneTest(false /* enable_nigori */);
}

TEST_F(DataTypeManagerImplTest, ConfigureOneStopWhileStarting) {
  DataTypeControllerMock* bookmark_dtc = MakeBookmarkDTC();
  SetBusyStartStopExpectations(bookmark_dtc,
                               DataTypeController::MODEL_STARTING);
  controllers_[syncable::BOOKMARKS] = bookmark_dtc;
  EXPECT_CALL(backend_, ConfigureDataTypes(_, _, _, _, true)).Times(1);
  DataTypeManagerImpl dtm(&backend_, &controllers_);
  types_.insert(syncable::BOOKMARKS);
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK);
  dtm.Configure(types_, sync_api::CONFIGURE_REASON_RECONFIGURATION);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm.state());
  dtm.Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}

TEST_F(DataTypeManagerImplTest, ConfigureOneStopWhileAssociating) {
  DataTypeControllerMock* bookmark_dtc = MakeBookmarkDTC();
  SetBusyStartStopExpectations(bookmark_dtc, DataTypeController::ASSOCIATING);
  controllers_[syncable::BOOKMARKS] = bookmark_dtc;
  EXPECT_CALL(backend_, ConfigureDataTypes(_, _, _, _, true)).Times(1);
  DataTypeManagerImpl dtm(&backend_, &controllers_);
  types_.insert(syncable::BOOKMARKS);
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK);
  dtm.Configure(types_, sync_api::CONFIGURE_REASON_RECONFIGURATION);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm.state());
  dtm.Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}

TEST_F(DataTypeManagerImplTest, OneWaitingForCrypto) {
  DataTypeControllerMock* password_dtc = MakePasswordDTC();
  EXPECT_CALL(*password_dtc, state()).Times(AtLeast(2)).
      WillRepeatedly(Return(DataTypeController::NOT_RUNNING));
  EXPECT_CALL(*password_dtc, Start(_)).
      WillOnce(InvokeCallback(syncable::PASSWORDS,
                              DataTypeController::NEEDS_CRYPTO));

  controllers_[syncable::PASSWORDS] = password_dtc;
  EXPECT_CALL(backend_, ConfigureDataTypes(_, _, _, _, true)).Times(1);

  DataTypeManagerImpl dtm(&backend_, &controllers_);
  types_.insert(syncable::PASSWORDS);
  SetConfigureStartExpectation();

  dtm.Configure(types_, sync_api::CONFIGURE_REASON_RECONFIGURATION);
  EXPECT_EQ(DataTypeManager::BLOCKED, dtm.state());

  Mock::VerifyAndClearExpectations(&backend_);
  Mock::VerifyAndClearExpectations(&observer_);
  Mock::VerifyAndClearExpectations(password_dtc);

  SetConfigureDoneExpectation(DataTypeManager::OK);
  SetPasswordDTCExpectations(password_dtc);
  EXPECT_CALL(*password_dtc, state()).
      WillOnce(Return(DataTypeController::NOT_RUNNING)).
      WillRepeatedly(Return(DataTypeController::RUNNING));
  EXPECT_CALL(*password_dtc, Start(_)).
      WillOnce(InvokeCallback(syncable::PASSWORDS,
                              DataTypeController::OK));
  EXPECT_CALL(backend_, ConfigureDataTypes(_, _, _, _, true)).Times(1);
  dtm.Configure(types_, sync_api::CONFIGURE_REASON_RECONFIGURATION);

  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm.state());
  EXPECT_CALL(*password_dtc, Stop()).Times(1);
  dtm.Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}

TEST_F(DataTypeManagerImplTest, ConfigureOneThenAnother) {
  DataTypeControllerMock* bookmark_dtc = MakeBookmarkDTC();
  SetStartStopExpectations(bookmark_dtc);
  controllers_[syncable::BOOKMARKS] = bookmark_dtc;
  DataTypeControllerMock* preference_dtc = MakePreferenceDTC();
  SetStartStopExpectations(preference_dtc);
  controllers_[syncable::PREFERENCES] = preference_dtc;

  EXPECT_CALL(backend_, ConfigureDataTypes(_, _, _, _, true)).Times(2);
  DataTypeManagerImpl dtm(&backend_, &controllers_);
  types_.insert(syncable::BOOKMARKS);

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK);
  dtm.Configure(types_, sync_api::CONFIGURE_REASON_RECONFIGURATION);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm.state());

  types_.insert(syncable::PREFERENCES);
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK);
  dtm.Configure(types_, sync_api::CONFIGURE_REASON_RECONFIGURATION);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm.state());

  dtm.Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}

TEST_F(DataTypeManagerImplTest, ConfigureOneThenSwitch) {
  DataTypeControllerMock* bookmark_dtc = MakeBookmarkDTC();
  SetStartStopExpectations(bookmark_dtc);
  controllers_[syncable::BOOKMARKS] = bookmark_dtc;
  DataTypeControllerMock* preference_dtc = MakePreferenceDTC();
  SetStartStopExpectations(preference_dtc);
  controllers_[syncable::PREFERENCES] = preference_dtc;

  EXPECT_CALL(backend_, ConfigureDataTypes(_, _, _, _, true)).Times(2);
  DataTypeManagerImpl dtm(&backend_, &controllers_);
  types_.insert(syncable::BOOKMARKS);

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK);
  dtm.Configure(types_, sync_api::CONFIGURE_REASON_RECONFIGURATION);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm.state());

  types_.clear();
  types_.insert(syncable::PREFERENCES);
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK);
  dtm.Configure(types_, sync_api::CONFIGURE_REASON_RECONFIGURATION);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm.state());

  dtm.Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}

TEST_F(DataTypeManagerImplTest, ConfigureWhileOneInFlight) {
  RunConfigureWhileOneInFlightTest(true /* enable_nigori */);
}

TEST_F(DataTypeManagerImplTest, ConfigureWithoutNigoriWhileOneInFlight) {
  RunConfigureWhileOneInFlightTest(false /* enable_nigori */);
}

TEST_F(DataTypeManagerImplTest, OneFailingController) {
  DataTypeControllerMock* bookmark_dtc = MakeBookmarkDTC();
  EXPECT_CALL(*bookmark_dtc, Start(_)).
      WillOnce(InvokeCallback(syncable::BOOKMARKS,
                              DataTypeController::UNRECOVERABLE_ERROR));
  EXPECT_CALL(*bookmark_dtc, Stop()).Times(0);
  EXPECT_CALL(*bookmark_dtc, state()).
      WillRepeatedly(Return(DataTypeController::NOT_RUNNING));
  controllers_[syncable::BOOKMARKS] = bookmark_dtc;

  DataTypeManagerImpl dtm(&backend_, &controllers_);
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::UNRECOVERABLE_ERROR);
  EXPECT_CALL(backend_, ConfigureDataTypes(_, _, _, _, true)).Times(1);

  types_.insert(syncable::BOOKMARKS);
  dtm.Configure(types_, sync_api::CONFIGURE_REASON_RECONFIGURATION);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}

TEST_F(DataTypeManagerImplTest, StopWhileInFlight) {
  DataTypeControllerMock* bookmark_dtc = MakeBookmarkDTC();
  SetStartStopExpectations(bookmark_dtc);
  controllers_[syncable::BOOKMARKS] = bookmark_dtc;

  DataTypeControllerMock* preference_dtc = MakePreferenceDTC();
  // Save the callback here so we can interrupt startup.
  DataTypeController::StartCallback* callback;
  EXPECT_CALL(*preference_dtc, Start(_)).
      WillOnce(SaveArg<0>(&callback));
  EXPECT_CALL(*preference_dtc, state()).
      WillRepeatedly(Return(DataTypeController::NOT_RUNNING));
  controllers_[syncable::PREFERENCES] = preference_dtc;

  DataTypeManagerImpl dtm(&backend_, &controllers_);
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::ABORTED);
  EXPECT_CALL(backend_, ConfigureDataTypes(_, _, _, _, true)).Times(1);

  types_.insert(syncable::BOOKMARKS);
  types_.insert(syncable::PREFERENCES);
  dtm.Configure(types_, sync_api::CONFIGURE_REASON_RECONFIGURATION);
  // Configure should stop in the CONFIGURING state because we are
  // waiting for the preferences callback to be invoked.
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm.state());

  // Call stop before the preference callback is invoked.
  EXPECT_CALL(*preference_dtc, Stop()).
      WillOnce(InvokeCallbackPointer(callback, syncable::PREFERENCES,
                                     DataTypeController::ABORTED));
  dtm.Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}

TEST_F(DataTypeManagerImplTest, SecondControllerFails) {
  DataTypeControllerMock* bookmark_dtc = MakeBookmarkDTC();
  SetStartStopExpectations(bookmark_dtc);
  controllers_[syncable::BOOKMARKS] = bookmark_dtc;

  DataTypeControllerMock* preference_dtc = MakePreferenceDTC();
  EXPECT_CALL(*preference_dtc, Start(_)).
      WillOnce(InvokeCallback(syncable::PREFERENCES,
                              DataTypeController::UNRECOVERABLE_ERROR));
  EXPECT_CALL(*preference_dtc, Stop()).Times(0);
  EXPECT_CALL(*preference_dtc, state()).
      WillRepeatedly(Return(DataTypeController::NOT_RUNNING));
  controllers_[syncable::PREFERENCES] = preference_dtc;

  DataTypeManagerImpl dtm(&backend_, &controllers_);
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::UNRECOVERABLE_ERROR);
  EXPECT_CALL(backend_, ConfigureDataTypes(_, _, _, _, true)).Times(1);

  types_.insert(syncable::BOOKMARKS);
  types_.insert(syncable::PREFERENCES);
  dtm.Configure(types_, sync_api::CONFIGURE_REASON_RECONFIGURATION);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}

ACTION_P(SetDisabledState, state) {
  *state = DataTypeController::DISABLED;
}

ACTION_P(ReturnState, state) {
  return *state;
}

TEST_F(DataTypeManagerImplTest, OneControllerFailsAssociation) {
  DataTypeControllerMock* bookmark_dtc = MakeBookmarkDTC();
  SetStartStopExpectations(bookmark_dtc);
  controllers_[syncable::BOOKMARKS] = bookmark_dtc;

  DataTypeController::State state = DataTypeController::NOT_RUNNING;

  DataTypeControllerMock* preference_dtc = MakePreferenceDTC();
  EXPECT_CALL(*preference_dtc, Start(_)).
      WillOnce(DoAll(SetDisabledState(&state),
                     InvokeCallback(syncable::PREFERENCES,
                         DataTypeController::ASSOCIATION_FAILED)));
  EXPECT_CALL(*preference_dtc, Stop()).Times(1);
  EXPECT_CALL(*preference_dtc, state()).
      WillRepeatedly(ReturnState(&state));
  controllers_[syncable::PREFERENCES] = preference_dtc;

  DataTypeManagerImpl dtm(&backend_, &controllers_);
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::PARTIAL_SUCCESS);
  EXPECT_CALL(backend_, ConfigureDataTypes(_, _, _, _, true)).Times(1);

  types_.insert(syncable::BOOKMARKS);
  types_.insert(syncable::PREFERENCES);
  dtm.Configure(types_, sync_api::CONFIGURE_REASON_RECONFIGURATION);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm.state());

  dtm.Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}


TEST_F(DataTypeManagerImplTest, ConfigureWhileDownloadPending) {
  RunConfigureWhileDownloadPendingTest(
      true /* enable_nigori */,
      syncable::ModelTypeSet() /* first_configure_result */);
}

TEST_F(DataTypeManagerImplTest, ConfigureWhileDownloadPendingWithoutNigori) {
  RunConfigureWhileDownloadPendingTest(
      false /* enable_nigori */,
      syncable::ModelTypeSet() /* first_configure_result */);
}

TEST_F(DataTypeManagerImplTest, ConfigureWhileDownloadPendingFail) {
  syncable::ModelTypeSet first_configure_result;
  first_configure_result.insert(syncable::BOOKMARKS);
  RunConfigureWhileDownloadPendingTest(
      true /* enable_nigori */, first_configure_result);
}

TEST_F(DataTypeManagerImplTest,
       ConfigureWhileDownloadPendingFailWithoutNigori) {
  syncable::ModelTypeSet first_configure_result;
  first_configure_result.insert(syncable::BOOKMARKS);
  RunConfigureWhileDownloadPendingTest(
      false /* enable_nigori */, first_configure_result);
}

TEST_F(DataTypeManagerImplTest, StopWhileDownloadPending) {
  DataTypeControllerMock* bookmark_dtc = MakeBookmarkDTC();
  SetNotUsedExpectations(bookmark_dtc);
  controllers_[syncable::BOOKMARKS] = bookmark_dtc;

  DataTypeManagerImpl dtm(&backend_, &controllers_);
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::ABORTED);
  base::Callback<void(const syncable::ModelTypeSet&)> task;
  // Grab the task the first time this is called so we can stop
  // before it is finished.
  EXPECT_CALL(backend_, ConfigureDataTypes(_, _, _, _, true)).
      WillOnce(SaveArg<3>(&task));

  types_.insert(syncable::BOOKMARKS);
  dtm.Configure(types_, sync_api::CONFIGURE_REASON_RECONFIGURATION);
  // Configure should stop in the DOWNLOAD_PENDING state because we
  // are waiting for the download ready task to be run.
  EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm.state());

  dtm.Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());

  // It should be perfectly safe to run this task even though the DTM
  // has been stopped.
  task.Run(syncable::ModelTypeSet());
}
