// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <set>

#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/stl_util-inl.h"
#include "base/task.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/glue/data_type_controller_mock.h"
#include "chrome/browser/sync/glue/data_type_manager_impl.h"
#include "chrome/browser/sync/glue/sync_backend_host_mock.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_observer_mock.h"
#include "chrome/common/notification_registrar.h"
#include "chrome/common/notification_service.h"
#include "chrome/common/notification_type.h"
#include "content/browser/browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using browser_sync::DataTypeManager;
using browser_sync::DataTypeManagerImpl;
using browser_sync::DataTypeController;
using browser_sync::DataTypeControllerMock;
using browser_sync::SyncBackendHostMock;
using testing::_;
using testing::DoAll;
using testing::DoDefault;
using testing::InSequence;
using testing::Property;
using testing::Pointee;
using testing::Return;
using testing::SaveArg;

ACTION_P(InvokeCallback, callback_result) {
  arg0->Run(callback_result);
  delete arg0;
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
                   NotificationType::SYNC_CONFIGURE_START,
                   NotificationService::AllSources());
    registrar_.Add(&observer_,
                   NotificationType::SYNC_CONFIGURE_DONE,
                   NotificationService::AllSources());
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
    EXPECT_CALL(*dtc, enabled()).WillRepeatedly(Return(true));
    EXPECT_CALL(*dtc, type()).WillRepeatedly(Return(syncable::PASSWORDS));
    EXPECT_CALL(*dtc, name()).WillRepeatedly(Return("passwords"));
    return dtc;
  }

  void SetStartStopExpectations(DataTypeControllerMock* mock_dtc) {
    InSequence seq;
    EXPECT_CALL(*mock_dtc, state()).
        WillRepeatedly(Return(DataTypeController::NOT_RUNNING));
    EXPECT_CALL(*mock_dtc, Start(_)).
        WillOnce(InvokeCallback((DataTypeController::OK)));
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
        WillOnce(InvokeCallback((DataTypeController::OK)));
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
        Observe(NotificationType(NotificationType::SYNC_CONFIGURE_START),
                _, _));
  }

  void SetConfigureDoneExpectation(DataTypeManager::ConfigureResult result) {
    EXPECT_CALL(
        observer_,
        Observe(NotificationType(NotificationType::SYNC_CONFIGURE_DONE), _,
                Property(&Details<DataTypeManager::ConfigureResult>::ptr,
                         Pointee(result))));
  }

  void SetBackendExpectations(int times) {
    EXPECT_CALL(backend_, UpdateEnabledTypes(_)).Times(times);
    EXPECT_CALL(backend_, ConfigureDataTypes(_, _, _)).Times(times);
    EXPECT_CALL(backend_, StartSyncingWithServer()).Times(times);
    EXPECT_CALL(backend_, RequestPause()).Times(times);
    EXPECT_CALL(backend_, RequestResume()).Times(times);
  }

  MessageLoopForUI message_loop_;
  BrowserThread ui_thread_;
  DataTypeController::TypeMap controllers_;
  SyncBackendHostMock backend_;
  NotificationObserverMock observer_;
  NotificationRegistrar registrar_;
  std::set<syncable::ModelType> types_;
};

TEST_F(DataTypeManagerImplTest, NoControllers) {
  DataTypeManagerImpl dtm(&backend_, controllers_);
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK);
  EXPECT_CALL(backend_, UpdateEnabledTypes(_));
  dtm.Configure(types_);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm.state());
  dtm.Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}

TEST_F(DataTypeManagerImplTest, ConfigureOne) {
  DataTypeControllerMock* bookmark_dtc = MakeBookmarkDTC();
  SetStartStopExpectations(bookmark_dtc);
  controllers_[syncable::BOOKMARKS] = bookmark_dtc;
  SetBackendExpectations(1);
  DataTypeManagerImpl dtm(&backend_, controllers_);
  types_.insert(syncable::BOOKMARKS);
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK);
  dtm.Configure(types_);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm.state());
  dtm.Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}

TEST_F(DataTypeManagerImplTest, ConfigureOneStopWhileStarting) {
  DataTypeControllerMock* bookmark_dtc = MakeBookmarkDTC();
  SetBusyStartStopExpectations(bookmark_dtc,
                               DataTypeController::MODEL_STARTING);
  controllers_[syncable::BOOKMARKS] = bookmark_dtc;
  SetBackendExpectations(1);
  DataTypeManagerImpl dtm(&backend_, controllers_);
  types_.insert(syncable::BOOKMARKS);
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK);
  dtm.Configure(types_);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm.state());
  dtm.Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}

TEST_F(DataTypeManagerImplTest, ConfigureOneStopWhileAssociating) {
  DataTypeControllerMock* bookmark_dtc = MakeBookmarkDTC();
  SetBusyStartStopExpectations(bookmark_dtc, DataTypeController::ASSOCIATING);
  controllers_[syncable::BOOKMARKS] = bookmark_dtc;
  SetBackendExpectations(1);
  DataTypeManagerImpl dtm(&backend_, controllers_);
  types_.insert(syncable::BOOKMARKS);
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK);
  dtm.Configure(types_);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm.state());
  dtm.Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}

TEST_F(DataTypeManagerImplTest, OneWaitingForCrypto) {
  DataTypeControllerMock* password_dtc = MakePasswordDTC();
  EXPECT_CALL(*password_dtc, state()).
      WillRepeatedly(Return(DataTypeController::NOT_RUNNING));
  EXPECT_CALL(*password_dtc, Start(_)).
      WillOnce(InvokeCallback((DataTypeController::NEEDS_CRYPTO)));
  EXPECT_CALL(*password_dtc, state()).
      WillRepeatedly(Return(DataTypeController::NOT_RUNNING));

  controllers_[syncable::PASSWORDS] = password_dtc;
  SetBackendExpectations(1);

  DataTypeManagerImpl dtm(&backend_, controllers_);
  types_.insert(syncable::PASSWORDS);
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK);
  dtm.Configure(types_);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm.state());
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

  SetBackendExpectations(2);
  DataTypeManagerImpl dtm(&backend_, controllers_);
  types_.insert(syncable::BOOKMARKS);

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK);
  dtm.Configure(types_);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm.state());

  types_.insert(syncable::PREFERENCES);
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK);

  dtm.Configure(types_);
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

  SetBackendExpectations(2);
  DataTypeManagerImpl dtm(&backend_, controllers_);
  types_.insert(syncable::BOOKMARKS);

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK);
  dtm.Configure(types_);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm.state());

  types_.clear();
  types_.insert(syncable::PREFERENCES);
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK);
  dtm.Configure(types_);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm.state());

  dtm.Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}

TEST_F(DataTypeManagerImplTest, ConfigureWhileOneInFlight) {
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

  SetBackendExpectations(2);
  DataTypeManagerImpl dtm(&backend_, controllers_);
  types_.insert(syncable::BOOKMARKS);

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK);
  dtm.Configure(types_);

  // At this point, the bookmarks dtc should be in flight.  Add
  // preferences and continue starting bookmarks.
  types_.insert(syncable::PREFERENCES);
  dtm.Configure(types_);
  callback->Run(DataTypeController::OK);
  delete callback;

  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm.state());

  dtm.Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}

TEST_F(DataTypeManagerImplTest, ConfigureWhilePausePending) {
  DataTypeControllerMock* bookmark_dtc = MakeBookmarkDTC();
  SetStartStopExpectations(bookmark_dtc);
  controllers_[syncable::BOOKMARKS] = bookmark_dtc;
  DataTypeControllerMock* preference_dtc = MakePreferenceDTC();
  SetStartStopExpectations(preference_dtc);
  controllers_[syncable::PREFERENCES] = preference_dtc;

  // Don't notify the first time pause is called.
  EXPECT_CALL(backend_, ConfigureDataTypes(_, _, _)).Times(2);
  EXPECT_CALL(backend_, StartSyncingWithServer()).Times(2);
  EXPECT_CALL(backend_, RequestPause()).
      WillOnce(Return(true)).
      WillOnce(DoDefault());
  EXPECT_CALL(backend_, RequestResume()).Times(2);
  DataTypeManagerImpl dtm(&backend_, controllers_);
  types_.insert(syncable::BOOKMARKS);

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK);
  dtm.Configure(types_);
  EXPECT_EQ(DataTypeManager::PAUSE_PENDING, dtm.state());

  // Configure while pause pending.
  types_.insert(syncable::PREFERENCES);
  dtm.Configure(types_);

  // Should now be RESTARTING.
  EXPECT_EQ(DataTypeManager::RESTARTING, dtm.state());

  // Send the SYNC_PAUSED notification.  This will allow the DTM to
  // wake up and restart itself with the new configuration.
  NotificationService::current()->Notify(NotificationType::SYNC_PAUSED,
                                         NotificationService::AllSources(),
                                         NotificationService::NoDetails());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm.state());
  dtm.Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}

TEST_F(DataTypeManagerImplTest, StopWhilePausePending) {
  DataTypeControllerMock* bookmark_dtc = MakeBookmarkDTC();
  SetNotUsedExpectations(bookmark_dtc);
  controllers_[syncable::BOOKMARKS] = bookmark_dtc;

  EXPECT_CALL(backend_, ConfigureDataTypes(_, _, _)).Times(1);
  EXPECT_CALL(backend_, StartSyncingWithServer()).Times(1);
  // Never notify when RequestPause is called.
  EXPECT_CALL(backend_, RequestPause()).WillOnce(Return(true));
  EXPECT_CALL(backend_, RequestResume()).Times(0);
  DataTypeManagerImpl dtm(&backend_, controllers_);

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::ABORTED);
  types_.insert(syncable::BOOKMARKS);
  dtm.Configure(types_);
  EXPECT_EQ(DataTypeManager::PAUSE_PENDING, dtm.state());

  // Stop while pause pending.
  dtm.Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());

  // We should be able to safely handle a SYNC_PAUSED notification.
  NotificationService::current()->Notify(NotificationType::SYNC_PAUSED,
                                         NotificationService::AllSources(),
                                         NotificationService::NoDetails());
}

TEST_F(DataTypeManagerImplTest, ConfigureWhileResumePending) {
  DataTypeControllerMock* bookmark_dtc = MakeBookmarkDTC();
  SetStartStopExpectations(bookmark_dtc);
  controllers_[syncable::BOOKMARKS] = bookmark_dtc;
  DataTypeControllerMock* preference_dtc = MakePreferenceDTC();
  SetStartStopExpectations(preference_dtc);
  controllers_[syncable::PREFERENCES] = preference_dtc;

  EXPECT_CALL(backend_, ConfigureDataTypes(_, _, _)).Times(2);
  EXPECT_CALL(backend_, StartSyncingWithServer()).Times(2);
  EXPECT_CALL(backend_, RequestPause()).Times(2);
  // Don't notify the first time resume is called.
  EXPECT_CALL(backend_, RequestResume()).
      WillOnce(Return(true)).
      WillOnce(DoDefault());
  DataTypeManagerImpl dtm(&backend_, controllers_);
  types_.insert(syncable::BOOKMARKS);

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK);
  dtm.Configure(types_);
  EXPECT_EQ(DataTypeManager::RESUME_PENDING, dtm.state());

  // Configure while resume pending.
  types_.insert(syncable::PREFERENCES);
  dtm.Configure(types_);

  // Should now be RESTARTING.
  EXPECT_EQ(DataTypeManager::RESTARTING, dtm.state());

  // Send the SYNC_PAUSED notification.  This will allow the DTM to
  // wake up and restart itself with the new configuration.
  NotificationService::current()->Notify(NotificationType::SYNC_RESUMED,
                                         NotificationService::AllSources(),
                                         NotificationService::NoDetails());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm.state());
  dtm.Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}

TEST_F(DataTypeManagerImplTest, StopWhileResumePending) {
  DataTypeControllerMock* bookmark_dtc = MakeBookmarkDTC();
  SetStartStopExpectations(bookmark_dtc);
  controllers_[syncable::BOOKMARKS] = bookmark_dtc;

  EXPECT_CALL(backend_, ConfigureDataTypes(_, _, _)).Times(1);
  EXPECT_CALL(backend_, StartSyncingWithServer()).Times(1);
  EXPECT_CALL(backend_, RequestPause()).Times(1);
  // Never notify pause resumed.
  EXPECT_CALL(backend_, RequestResume()).WillOnce(Return(true));
  DataTypeManagerImpl dtm(&backend_, controllers_);

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::ABORTED);
  types_.insert(syncable::BOOKMARKS);
  dtm.Configure(types_);
  EXPECT_EQ(DataTypeManager::RESUME_PENDING, dtm.state());

  // Stop while pause pending.
  dtm.Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());

  // We should be able to safely handle a SYNC_RESUMED notification.
  NotificationService::current()->Notify(NotificationType::SYNC_RESUMED,
                                         NotificationService::AllSources(),
                                         NotificationService::NoDetails());
}

TEST_F(DataTypeManagerImplTest, OneFailingController) {
  DataTypeControllerMock* bookmark_dtc = MakeBookmarkDTC();
  EXPECT_CALL(*bookmark_dtc, Start(_)).
      WillOnce(InvokeCallback((DataTypeController::ASSOCIATION_FAILED)));
  EXPECT_CALL(*bookmark_dtc, Stop()).Times(0);
  EXPECT_CALL(*bookmark_dtc, state()).
      WillRepeatedly(Return(DataTypeController::NOT_RUNNING));
  controllers_[syncable::BOOKMARKS] = bookmark_dtc;

  DataTypeManagerImpl dtm(&backend_, controllers_);
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::ASSOCIATION_FAILED);
  EXPECT_CALL(backend_, ConfigureDataTypes(_, _, _)).Times(1);
  EXPECT_CALL(backend_, StartSyncingWithServer()).Times(1);
  EXPECT_CALL(backend_, RequestPause()).Times(1);
  EXPECT_CALL(backend_, RequestResume()).Times(0);

  types_.insert(syncable::BOOKMARKS);
  dtm.Configure(types_);
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
  EXPECT_CALL(*preference_dtc, Stop()).Times(1);
  EXPECT_CALL(*preference_dtc, state()).
      WillRepeatedly(Return(DataTypeController::NOT_RUNNING));
  controllers_[syncable::PREFERENCES] = preference_dtc;

  DataTypeManagerImpl dtm(&backend_, controllers_);
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::ABORTED);
  EXPECT_CALL(backend_, ConfigureDataTypes(_, _, _)).Times(1);
  EXPECT_CALL(backend_, StartSyncingWithServer()).Times(1);
  EXPECT_CALL(backend_, RequestPause()).Times(1);
  EXPECT_CALL(backend_, RequestResume()).Times(0);

  types_.insert(syncable::BOOKMARKS);
  types_.insert(syncable::PREFERENCES);
  dtm.Configure(types_);
  // Configure should stop in the CONFIGURING state because we are
  // waiting for the preferences callback to be invoked.
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm.state());

  // Call stop before the preference callback is invoked.
  dtm.Stop();
  callback->Run(DataTypeController::ABORTED);
  delete callback;
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}

TEST_F(DataTypeManagerImplTest, SecondControllerFails) {
  DataTypeControllerMock* bookmark_dtc = MakeBookmarkDTC();
  SetStartStopExpectations(bookmark_dtc);
  controllers_[syncable::BOOKMARKS] = bookmark_dtc;

  DataTypeControllerMock* preference_dtc = MakePreferenceDTC();
  EXPECT_CALL(*preference_dtc, Start(_)).
      WillOnce(InvokeCallback((DataTypeController::ASSOCIATION_FAILED)));
  EXPECT_CALL(*preference_dtc, Stop()).Times(0);
  EXPECT_CALL(*preference_dtc, state()).
      WillRepeatedly(Return(DataTypeController::NOT_RUNNING));
  controllers_[syncable::PREFERENCES] = preference_dtc;

  DataTypeManagerImpl dtm(&backend_, controllers_);
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::ASSOCIATION_FAILED);
  EXPECT_CALL(backend_, ConfigureDataTypes(_, _, _)).Times(1);
  EXPECT_CALL(backend_, StartSyncingWithServer()).Times(1);
  EXPECT_CALL(backend_, RequestPause()).Times(1);
  EXPECT_CALL(backend_, RequestResume()).Times(0);

  types_.insert(syncable::BOOKMARKS);
  types_.insert(syncable::PREFERENCES);
  dtm.Configure(types_);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}

TEST_F(DataTypeManagerImplTest, PauseFailed) {
  DataTypeControllerMock* bookmark_dtc = MakeBookmarkDTC();
  EXPECT_CALL(*bookmark_dtc, Start(_)).Times(0);
  EXPECT_CALL(*bookmark_dtc, state()).
      WillRepeatedly(Return(DataTypeController::NOT_RUNNING));
  controllers_[syncable::BOOKMARKS] = bookmark_dtc;

  DataTypeManagerImpl dtm(&backend_, controllers_);
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::UNRECOVERABLE_ERROR);
  EXPECT_CALL(backend_, ConfigureDataTypes(_, _, _)).Times(1);
  EXPECT_CALL(backend_, StartSyncingWithServer()).Times(1);
  EXPECT_CALL(backend_, RequestPause()).WillOnce(Return(false));
  EXPECT_CALL(backend_, RequestResume()).Times(0);

  types_.insert(syncable::BOOKMARKS);
  dtm.Configure(types_);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}

TEST_F(DataTypeManagerImplTest, ResumeFailed) {
  DataTypeControllerMock* bookmark_dtc = MakeBookmarkDTC();
  SetStartStopExpectations(bookmark_dtc);
  controllers_[syncable::BOOKMARKS] = bookmark_dtc;

  DataTypeManagerImpl dtm(&backend_, controllers_);
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::UNRECOVERABLE_ERROR);
  EXPECT_CALL(backend_, ConfigureDataTypes(_, _, _)).Times(1);
  EXPECT_CALL(backend_, StartSyncingWithServer()).Times(1);
  EXPECT_CALL(backend_, RequestPause()).Times(1);
  EXPECT_CALL(backend_, RequestResume()).WillOnce(Return(false));

  types_.insert(syncable::BOOKMARKS);
  dtm.Configure(types_);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}

TEST_F(DataTypeManagerImplTest, ConfigureWhileDownloadPending) {
  DataTypeControllerMock* bookmark_dtc = MakeBookmarkDTC();
  SetStartStopExpectations(bookmark_dtc);
  controllers_[syncable::BOOKMARKS] = bookmark_dtc;

  DataTypeControllerMock* preference_dtc = MakePreferenceDTC();
  SetStartStopExpectations(preference_dtc);
  controllers_[syncable::PREFERENCES] = preference_dtc;

  DataTypeManagerImpl dtm(&backend_, controllers_);
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK);
  CancelableTask* task;
  // Grab the task the first time this is called so we can configure
  // before it is finished.
  EXPECT_CALL(backend_, ConfigureDataTypes(_, _, _)).
      WillOnce(SaveArg<2>(&task)).
      WillOnce(DoDefault());
  EXPECT_CALL(backend_, StartSyncingWithServer()).Times(2);
  EXPECT_CALL(backend_, RequestPause()).Times(1);
  EXPECT_CALL(backend_, RequestResume()).Times(1);

  types_.insert(syncable::BOOKMARKS);
  dtm.Configure(types_);
  // Configure should stop in the DOWNLOAD_PENDING state because we
  // are waiting for the download ready task to be run.
  EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm.state());

  types_.insert(syncable::PREFERENCES);
  dtm.Configure(types_);

  // Should now be RESTARTING.
  EXPECT_EQ(DataTypeManager::RESTARTING, dtm.state());

  // Running the task will queue a restart task to the message loop, and
  // eventually get us configured.
  task->Run();
  delete task;
  EXPECT_EQ(DataTypeManager::RESTARTING, dtm.state());
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm.state());

  dtm.Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}

TEST_F(DataTypeManagerImplTest, StopWhileDownloadPending) {
  DataTypeControllerMock* bookmark_dtc = MakeBookmarkDTC();
  SetNotUsedExpectations(bookmark_dtc);
  controllers_[syncable::BOOKMARKS] = bookmark_dtc;

  DataTypeManagerImpl dtm(&backend_, controllers_);
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::ABORTED);
  CancelableTask* task;
  // Grab the task the first time this is called so we can stop
  // before it is finished.
  EXPECT_CALL(backend_, ConfigureDataTypes(_, _, _)).
      WillOnce(SaveArg<2>(&task));
  EXPECT_CALL(backend_, StartSyncingWithServer());
  EXPECT_CALL(backend_, RequestPause()).Times(0);
  EXPECT_CALL(backend_, RequestResume()).Times(0);

  types_.insert(syncable::BOOKMARKS);
  dtm.Configure(types_);
  // Configure should stop in the DOWNLOAD_PENDING state because we
  // are waiting for the download ready task to be run.
  EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm.state());

  dtm.Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());

  // It should be perfectly safe to run this task even though the DTM
  // has been stopped.
  task->Run();
  delete task;
}
