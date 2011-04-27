// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/data_type_manager_impl.h"

#include <set>

#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/stl_util-inl.h"
#include "base/task.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/glue/data_type_controller_mock.h"
#include "chrome/browser/sync/glue/sync_backend_host_mock.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/browser/sync/syncable/model_type.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_details.h"
#include "content/common/notification_observer_mock.h"
#include "content/common/notification_registrar.h"
#include "content/common/notification_service.h"
#include "content/common/notification_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using browser_sync::DataTypeManager;
using browser_sync::DataTypeManagerImpl;
using browser_sync::DataTypeController;
using browser_sync::DataTypeControllerMock;
using browser_sync::SyncBackendHostMock;
using testing::_;
using testing::AtLeast;
using testing::DoAll;
using testing::DoDefault;
using testing::InSequence;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::InvokeWithoutArgs;
using testing::Mock;
using testing::Property;
using testing::Pointee;
using testing::Return;
using testing::SaveArg;

ACTION_P(InvokeCallback, callback_result) {
  arg0->Run(callback_result, FROM_HERE);
  delete arg0;
}

ACTION_P2(InvokeCallbackPointer, callback, argument) {
  callback->Run(argument, FROM_HERE);
  delete callback;
}

DataTypeManager::ConfigureResult GetResult(
    const NotificationDetails& details) {
  DataTypeManager::ConfigureResultWithErrorLocation* result_with_location =
      Details<DataTypeManager::ConfigureResultWithErrorLocation>(
      details).ptr();
  return result_with_location->result;
}

class DataTypeManagerImpl2Test : public testing::Test {
 public:
  DataTypeManagerImpl2Test()
      : ui_thread_(BrowserThread::UI, &message_loop_) {}

  virtual ~DataTypeManagerImpl2Test() {
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
        ::testing::ResultOf(&GetResult, result)));
  }

  MessageLoopForUI message_loop_;
  BrowserThread ui_thread_;
  DataTypeController::TypeMap controllers_;
  SyncBackendHostMock backend_;
  NotificationObserverMock observer_;
  NotificationRegistrar registrar_;
  std::set<syncable::ModelType> types_;
};

TEST_F(DataTypeManagerImpl2Test, NoControllers) {
  DataTypeManagerImpl dtm(&backend_, controllers_);
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK);
  dtm.Configure(types_);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm.state());
  dtm.Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}

TEST_F(DataTypeManagerImpl2Test, ConfigureOne) {
  DataTypeControllerMock* bookmark_dtc = MakeBookmarkDTC();
  SetStartStopExpectations(bookmark_dtc);
  controllers_[syncable::BOOKMARKS] = bookmark_dtc;
  EXPECT_CALL(backend_, ConfigureDataTypes(_, _, _)).Times(1);
  DataTypeManagerImpl dtm(&backend_, controllers_);
  types_.insert(syncable::BOOKMARKS);
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK);
  dtm.Configure(types_);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm.state());
  dtm.Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}

TEST_F(DataTypeManagerImpl2Test, ConfigureOneStopWhileStarting) {
  DataTypeControllerMock* bookmark_dtc = MakeBookmarkDTC();
  SetBusyStartStopExpectations(bookmark_dtc,
                               DataTypeController::MODEL_STARTING);
  controllers_[syncable::BOOKMARKS] = bookmark_dtc;
  EXPECT_CALL(backend_, ConfigureDataTypes(_, _, _)).Times(1);
  DataTypeManagerImpl dtm(&backend_, controllers_);
  types_.insert(syncable::BOOKMARKS);
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK);
  dtm.Configure(types_);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm.state());
  dtm.Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}

TEST_F(DataTypeManagerImpl2Test, ConfigureOneStopWhileAssociating) {
  DataTypeControllerMock* bookmark_dtc = MakeBookmarkDTC();
  SetBusyStartStopExpectations(bookmark_dtc, DataTypeController::ASSOCIATING);
  controllers_[syncable::BOOKMARKS] = bookmark_dtc;
  EXPECT_CALL(backend_, ConfigureDataTypes(_, _, _)).Times(1);
  DataTypeManagerImpl dtm(&backend_, controllers_);
  types_.insert(syncable::BOOKMARKS);
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK);
  dtm.Configure(types_);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm.state());
  dtm.Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}

TEST_F(DataTypeManagerImpl2Test, OneWaitingForCrypto) {
  DataTypeControllerMock* password_dtc = MakePasswordDTC();
  EXPECT_CALL(*password_dtc, state()).Times(AtLeast(2)).
      WillRepeatedly(Return(DataTypeController::NOT_RUNNING));
  EXPECT_CALL(*password_dtc, Start(_)).
      WillOnce(InvokeCallback((DataTypeController::NEEDS_CRYPTO)));

  controllers_[syncable::PASSWORDS] = password_dtc;
  EXPECT_CALL(backend_, ConfigureDataTypes(_, _, _)).Times(1);

  DataTypeManagerImpl dtm(&backend_, controllers_);
  types_.insert(syncable::PASSWORDS);
  SetConfigureStartExpectation();

  dtm.Configure(types_);
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
      WillOnce(InvokeCallback((DataTypeController::OK)));
  EXPECT_CALL(backend_, ConfigureDataTypes(_, _, _)).Times(1);
  dtm.Configure(types_);

  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm.state());
  EXPECT_CALL(*password_dtc, Stop()).Times(1);
  dtm.Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}

TEST_F(DataTypeManagerImpl2Test, ConfigureOneThenAnother) {
  DataTypeControllerMock* bookmark_dtc = MakeBookmarkDTC();
  SetStartStopExpectations(bookmark_dtc);
  controllers_[syncable::BOOKMARKS] = bookmark_dtc;
  DataTypeControllerMock* preference_dtc = MakePreferenceDTC();
  SetStartStopExpectations(preference_dtc);
  controllers_[syncable::PREFERENCES] = preference_dtc;

  EXPECT_CALL(backend_, ConfigureDataTypes(_, _, _)).Times(2);
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

TEST_F(DataTypeManagerImpl2Test, ConfigureOneThenSwitch) {
  DataTypeControllerMock* bookmark_dtc = MakeBookmarkDTC();
  SetStartStopExpectations(bookmark_dtc);
  controllers_[syncable::BOOKMARKS] = bookmark_dtc;
  DataTypeControllerMock* preference_dtc = MakePreferenceDTC();
  SetStartStopExpectations(preference_dtc);
  controllers_[syncable::PREFERENCES] = preference_dtc;

  EXPECT_CALL(backend_, ConfigureDataTypes(_, _, _)).Times(2);
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

void DoConfigureDataTypes(
    const DataTypeController::TypeMap& data_type_controllers,
    const syncable::ModelTypeSet& types,
    CancelableTask* ready_task) {
  ready_task->Run();
  delete ready_task;
}

void QuitMessageLoop() {
  MessageLoop::current()->Quit();
}

TEST_F(DataTypeManagerImpl2Test, ConfigureWhileOneInFlight) {
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

  DataTypeManagerImpl dtm(&backend_, controllers_);
  EXPECT_CALL(backend_, ConfigureDataTypes(_, _, _))
    .WillOnce(Invoke(DoConfigureDataTypes))
    .WillOnce(DoAll(Invoke(DoConfigureDataTypes),
     InvokeWithoutArgs(QuitMessageLoop)));

  types_.insert(syncable::BOOKMARKS);

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK);
  dtm.Configure(types_);

  // At this point, the bookmarks dtc should be in flight.  Add
  // preferences and continue starting bookmarks.
  types_.insert(syncable::PREFERENCES);
  dtm.Configure(types_);
  callback->Run(DataTypeController::OK, FROM_HERE);
  delete callback;

  MessageLoop::current()->Run();

  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm.state());

  dtm.Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}

TEST_F(DataTypeManagerImpl2Test, OneFailingController) {
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

  types_.insert(syncable::BOOKMARKS);
  dtm.Configure(types_);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}

TEST_F(DataTypeManagerImpl2Test, StopWhileInFlight) {
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

  DataTypeManagerImpl dtm(&backend_, controllers_);
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::ABORTED);
  EXPECT_CALL(backend_, ConfigureDataTypes(_, _, _)).Times(1);

  types_.insert(syncable::BOOKMARKS);
  types_.insert(syncable::PREFERENCES);
  dtm.Configure(types_);
  // Configure should stop in the CONFIGURING state because we are
  // waiting for the preferences callback to be invoked.
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm.state());

  // Call stop before the preference callback is invoked.
  EXPECT_CALL(*preference_dtc, Stop()).
      WillOnce(InvokeCallbackPointer(callback, DataTypeController::ABORTED));
  dtm.Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}

TEST_F(DataTypeManagerImpl2Test, SecondControllerFails) {
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

  types_.insert(syncable::BOOKMARKS);
  types_.insert(syncable::PREFERENCES);
  dtm.Configure(types_);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}

TEST_F(DataTypeManagerImpl2Test, ConfigureWhileDownloadPending) {
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

  types_.insert(syncable::BOOKMARKS);
  dtm.Configure(types_);
  // Configure should stop in the DOWNLOAD_PENDING state because we
  // are waiting for the download ready task to be run.
  EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm.state());

  types_.insert(syncable::PREFERENCES);
  dtm.Configure(types_);

  // Running the task will queue a restart task to the message loop, and
  // eventually get us configured.
  task->Run();
  delete task;
  MessageLoop::current()->RunAllPending();
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm.state());

  dtm.Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}

TEST_F(DataTypeManagerImpl2Test, StopWhileDownloadPending) {
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
