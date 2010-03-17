// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"
#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/stl_util-inl.h"
#include "base/task.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/glue/data_type_controller_mock.h"
#include "chrome/browser/sync/glue/data_type_manager_impl.h"
#include "chrome/browser/sync/glue/sync_backend_host_mock.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/common/notification_type.h"
#include "testing/gmock/include/gmock/gmock.h"

using browser_sync::DataTypeManager;
using browser_sync::DataTypeManagerImpl;
using browser_sync::DataTypeController;
using browser_sync::DataTypeControllerMock;
using browser_sync::SyncBackendHostMock;
using testing::_;
using testing::DoAll;
using testing::Return;
using testing::SaveArg;

ACTION_P(InvokeCallback, callback_result) {
  arg1->Run(callback_result);
  delete arg1;
}

class StartCallback {
 public:
  MOCK_METHOD1(Run, void(DataTypeManager::StartResult result));
};

class DataTypeManagerImplTest : public testing::Test {
 public:
  DataTypeManagerImplTest()
      : ui_thread_(ChromeThread::UI, &message_loop_) {}

  virtual ~DataTypeManagerImplTest() {
  }

 protected:
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

  void SetStartStopExpectations(DataTypeControllerMock* mock_dtc) {
    EXPECT_CALL(*mock_dtc, Start(true, _)).
        WillOnce(InvokeCallback((DataTypeController::OK)));
    EXPECT_CALL(*mock_dtc, Stop()).Times(1);

    // The state() getter is used a few times in DCHECK code and needs
    // different mock programming in debug vs. release mode.
#ifdef NDEBUG
    EXPECT_CALL(*mock_dtc, state()).
        WillOnce(Return(DataTypeController::RUNNING));
#else
    EXPECT_CALL(*mock_dtc, state()).
        WillOnce(Return(DataTypeController::NOT_RUNNING)).
        WillOnce(Return(DataTypeController::RUNNING)).
        WillOnce(Return(DataTypeController::RUNNING));
#endif
  }

  MessageLoopForUI message_loop_;
  ChromeThread ui_thread_;
  DataTypeController::TypeMap controllers_;
  StartCallback callback_;
  SyncBackendHostMock backend_;
};

TEST_F(DataTypeManagerImplTest, NoControllers) {
  DataTypeManagerImpl dtm(&backend_, controllers_);
  EXPECT_CALL(callback_, Run(DataTypeManager::OK));
  dtm.Start(NewCallback(&callback_, &StartCallback::Run));
  EXPECT_EQ(DataTypeManager::STARTED, dtm.state());
  dtm.Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}

TEST_F(DataTypeManagerImplTest, OneDisabledController) {
  DataTypeControllerMock* bookmark_dtc = MakeBookmarkDTC();
  EXPECT_CALL(*bookmark_dtc, enabled()).WillRepeatedly(Return(false));
  EXPECT_CALL(*bookmark_dtc, Start(_, _)).Times(0);
  EXPECT_CALL(*bookmark_dtc, Stop()).Times(0);
  EXPECT_CALL(*bookmark_dtc, state()).
      WillRepeatedly(Return(DataTypeController::NOT_RUNNING));
  controllers_[syncable::BOOKMARKS] = bookmark_dtc;

  DataTypeManagerImpl dtm(&backend_, controllers_);
  EXPECT_CALL(callback_, Run(DataTypeManager::OK));
  dtm.Start(NewCallback(&callback_, &StartCallback::Run));
  EXPECT_EQ(DataTypeManager::STARTED, dtm.state());
  dtm.Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}

TEST_F(DataTypeManagerImplTest, OneEnabledController) {
  DataTypeControllerMock* bookmark_dtc = MakeBookmarkDTC();
  SetStartStopExpectations(bookmark_dtc);
  controllers_[syncable::BOOKMARKS] = bookmark_dtc;

  DataTypeManagerImpl dtm(&backend_, controllers_);
  EXPECT_CALL(callback_, Run(DataTypeManager::OK));
  dtm.Start(NewCallback(&callback_, &StartCallback::Run));
  EXPECT_EQ(DataTypeManager::STARTED, dtm.state());
  dtm.Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}

TEST_F(DataTypeManagerImplTest, OneFailingController) {
  DataTypeControllerMock* bookmark_dtc = MakeBookmarkDTC();
  EXPECT_CALL(*bookmark_dtc, Start(true, _)).
      WillOnce(InvokeCallback((DataTypeController::ASSOCIATION_FAILED)));
  EXPECT_CALL(*bookmark_dtc, Stop()).Times(0);
  // The state() getter is used a few times in DCHECK code and needs
  // different mock programming in debug vs. release mode.
  EXPECT_CALL(*bookmark_dtc, state()).
      WillRepeatedly(Return(DataTypeController::NOT_RUNNING));
  controllers_[syncable::BOOKMARKS] = bookmark_dtc;

  DataTypeManagerImpl dtm(&backend_, controllers_);
  EXPECT_CALL(backend_, RequestPause()).
      WillOnce(DoAll(Notify(NotificationType::SYNC_PAUSED), Return(true)));
  EXPECT_CALL(callback_, Run(DataTypeManager::ASSOCIATION_FAILED));
  dtm.Start(NewCallback(&callback_, &StartCallback::Run));
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}

TEST_F(DataTypeManagerImplTest, TwoEnabledControllers) {
  DataTypeControllerMock* bookmark_dtc = MakeBookmarkDTC();
  SetStartStopExpectations(bookmark_dtc);
  controllers_[syncable::BOOKMARKS] = bookmark_dtc;

  DataTypeControllerMock* preference_dtc = MakePreferenceDTC();
  SetStartStopExpectations(preference_dtc);
  controllers_[syncable::PREFERENCES] = preference_dtc;

  DataTypeManagerImpl dtm(&backend_, controllers_);
  EXPECT_CALL(callback_, Run(DataTypeManager::OK));
  dtm.Start(NewCallback(&callback_, &StartCallback::Run));
  EXPECT_EQ(DataTypeManager::STARTED, dtm.state());
  dtm.Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}

TEST_F(DataTypeManagerImplTest, InterruptedStart) {
   DataTypeControllerMock* bookmark_dtc = MakeBookmarkDTC();
  SetStartStopExpectations(bookmark_dtc);
  controllers_[syncable::BOOKMARKS] = bookmark_dtc;

  DataTypeControllerMock* preference_dtc = MakePreferenceDTC();
  // Save the callback here so we can interrupt startup.
  DataTypeController::StartCallback* callback;
  EXPECT_CALL(*preference_dtc, Start(true, _)).
      WillOnce(SaveArg<1>(&callback));
  EXPECT_CALL(*preference_dtc, Stop()).Times(1);
  EXPECT_CALL(*preference_dtc, state()).
      WillRepeatedly(Return(DataTypeController::NOT_RUNNING));
  controllers_[syncable::PREFERENCES] = preference_dtc;

  DataTypeManagerImpl dtm(&backend_, controllers_);
  EXPECT_CALL(backend_, RequestPause()).
      WillOnce(DoAll(Notify(NotificationType::SYNC_PAUSED), Return(true)));
  EXPECT_CALL(callback_, Run(DataTypeManager::ABORTED));
  dtm.Start(NewCallback(&callback_, &StartCallback::Run));
  EXPECT_EQ(DataTypeManager::STARTING, dtm.state());

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
  EXPECT_CALL(*preference_dtc, Start(true, _)).
      WillOnce(InvokeCallback((DataTypeController::ASSOCIATION_FAILED)));
  EXPECT_CALL(*preference_dtc, Stop()).Times(0);
  EXPECT_CALL(*preference_dtc, state()).
      WillRepeatedly(Return(DataTypeController::NOT_RUNNING));
  controllers_[syncable::PREFERENCES] = preference_dtc;

  DataTypeManagerImpl dtm(&backend_, controllers_);
  EXPECT_CALL(backend_, RequestPause()).
      WillOnce(DoAll(Notify(NotificationType::SYNC_PAUSED), Return(true)));
  EXPECT_CALL(callback_, Run(DataTypeManager::ASSOCIATION_FAILED));
  dtm.Start(NewCallback(&callback_, &StartCallback::Run));
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}

TEST_F(DataTypeManagerImplTest, IsRegisteredNone) {
  DataTypeManagerImpl dtm(&backend_, controllers_);
  EXPECT_FALSE(dtm.IsRegistered(syncable::BOOKMARKS));
}

TEST_F(DataTypeManagerImplTest, IsRegisteredButNoMatch) {
  DataTypeControllerMock* preference_dtc = MakePreferenceDTC();
  EXPECT_CALL(*preference_dtc, state()).
      WillRepeatedly(Return(DataTypeController::NOT_RUNNING));
  controllers_[syncable::PREFERENCES] = preference_dtc;
  DataTypeManagerImpl dtm(&backend_, controllers_);
  EXPECT_FALSE(dtm.IsRegistered(syncable::BOOKMARKS));
}

TEST_F(DataTypeManagerImplTest, IsRegisteredMatch) {
  DataTypeControllerMock* bookmark_dtc = MakeBookmarkDTC();
  EXPECT_CALL(*bookmark_dtc, state()).
      WillRepeatedly(Return(DataTypeController::NOT_RUNNING));
  controllers_[syncable::BOOKMARKS] = bookmark_dtc;
  DataTypeManagerImpl dtm(&backend_, controllers_);
  EXPECT_TRUE(dtm.IsRegistered(syncable::BOOKMARKS));
}

TEST_F(DataTypeManagerImplTest, IsNotEnabled) {
  DataTypeControllerMock* bookmark_dtc = MakeBookmarkDTC();
  EXPECT_CALL(*bookmark_dtc, state()).
      WillRepeatedly(Return(DataTypeController::NOT_RUNNING));
  EXPECT_CALL(*bookmark_dtc, enabled()).
      WillRepeatedly(Return(false));
  controllers_[syncable::BOOKMARKS] = bookmark_dtc;
  DataTypeManagerImpl dtm(&backend_, controllers_);
  EXPECT_FALSE(dtm.IsEnabled(syncable::BOOKMARKS));
}

TEST_F(DataTypeManagerImplTest, IsEnabled) {
  DataTypeControllerMock* bookmark_dtc = MakeBookmarkDTC();
  EXPECT_CALL(*bookmark_dtc, state()).
      WillRepeatedly(Return(DataTypeController::NOT_RUNNING));
  EXPECT_CALL(*bookmark_dtc, enabled()).
      WillRepeatedly(Return(true));
  controllers_[syncable::BOOKMARKS] = bookmark_dtc;
  DataTypeManagerImpl dtm(&backend_, controllers_);
  EXPECT_TRUE(dtm.IsEnabled(syncable::BOOKMARKS));
}

TEST_F(DataTypeManagerImplTest, PauseFailed) {
  DataTypeControllerMock* bookmark_dtc = MakeBookmarkDTC();
  EXPECT_CALL(*bookmark_dtc, Start(_, _)).Times(0);
  EXPECT_CALL(*bookmark_dtc, state()).
      WillRepeatedly(Return(DataTypeController::NOT_RUNNING));
  controllers_[syncable::BOOKMARKS] = bookmark_dtc;

  DataTypeManagerImpl dtm(&backend_, controllers_);
  EXPECT_CALL(backend_, RequestPause()).
      WillOnce(Return(false));

  EXPECT_CALL(callback_, Run(DataTypeManager::UNRECOVERABLE_ERROR));
  dtm.Start(NewCallback(&callback_, &StartCallback::Run));
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}

TEST_F(DataTypeManagerImplTest, ResumeFailed) {
  DataTypeControllerMock* bookmark_dtc = MakeBookmarkDTC();
  SetStartStopExpectations(bookmark_dtc);
  controllers_[syncable::BOOKMARKS] = bookmark_dtc;

  DataTypeManagerImpl dtm(&backend_, controllers_);
  EXPECT_CALL(backend_, RequestPause()).
      WillOnce(DoAll(Notify(NotificationType::SYNC_PAUSED), Return(true)));
  EXPECT_CALL(backend_, RequestResume()).
      WillOnce(Return(false));

  EXPECT_CALL(callback_, Run(DataTypeManager::UNRECOVERABLE_ERROR));
  dtm.Start(NewCallback(&callback_, &StartCallback::Run));
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}
