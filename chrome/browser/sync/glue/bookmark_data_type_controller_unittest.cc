// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/glue/bookmark_data_type_controller.h"
#include "chrome/browser/sync/glue/change_processor_mock.h"
#include "chrome/browser/sync/glue/data_type_controller_mock.h"
#include "chrome/browser/sync/glue/model_associator_mock.h"
#include "chrome/browser/sync/profile_sync_components_factory_mock.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/profile_mock.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/test/test_browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"

using browser_sync::BookmarkDataTypeController;
using browser_sync::ChangeProcessorMock;
using browser_sync::DataTypeController;
using browser_sync::ModelAssociatorMock;
using browser_sync::StartCallback;
using content::BrowserThread;
using testing::_;
using testing::DoAll;
using testing::InvokeWithoutArgs;
using testing::Return;
using testing::SetArgumentPointee;

class BookmarkModelMock : public BookmarkModel {
 public:
  BookmarkModelMock() : BookmarkModel(NULL) {}
  MOCK_CONST_METHOD0(IsLoaded, bool(void));
};

class BookmarkDataTypeControllerTest : public testing::Test {
 public:
  BookmarkDataTypeControllerTest()
      : ui_thread_(BrowserThread::UI, &message_loop_) {}

  virtual void SetUp() {
    model_associator_ = new ModelAssociatorMock();
    change_processor_ = new ChangeProcessorMock();
    profile_sync_factory_.reset(
        new ProfileSyncComponentsFactoryMock(model_associator_,
                                   change_processor_));
    bookmark_dtc_ =
        new BookmarkDataTypeController(profile_sync_factory_.get(),
                                       &profile_,
                                       &service_);
  }

 protected:
  void SetStartExpectations() {
    EXPECT_CALL(profile_, GetBookmarkModel()).
        WillOnce(Return(&bookmark_model_));
    EXPECT_CALL(bookmark_model_, IsLoaded()).WillRepeatedly(Return(true));
  }

  void SetAssociateExpectations() {
    EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary()).
        WillRepeatedly(Return(true));
    EXPECT_CALL(*profile_sync_factory_, CreateBookmarkSyncComponents(_, _));
    EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
        WillRepeatedly(DoAll(SetArgumentPointee<0>(true), Return(true)));
    EXPECT_CALL(*model_associator_, AssociateModels(_)).
        WillRepeatedly(Return(true));
    EXPECT_CALL(service_, ActivateDataType(_, _, _));
  }

  void SetStopExpectations() {
    EXPECT_CALL(service_, DeactivateDataType(_));
    EXPECT_CALL(*model_associator_, DisassociateModels(_));
  }

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  scoped_refptr<BookmarkDataTypeController> bookmark_dtc_;
  scoped_ptr<ProfileSyncComponentsFactoryMock> profile_sync_factory_;
  ProfileMock profile_;
  BookmarkModelMock bookmark_model_;
  ProfileSyncServiceMock service_;
  ModelAssociatorMock* model_associator_;
  ChangeProcessorMock* change_processor_;
  StartCallback start_callback_;

  void PumpLoop() {
    message_loop_.RunAllPending();
  }
};

TEST_F(BookmarkDataTypeControllerTest, StartBookmarkModelReady) {
  SetStartExpectations();
  SetAssociateExpectations();

  EXPECT_EQ(DataTypeController::NOT_RUNNING, bookmark_dtc_->state());

  EXPECT_CALL(start_callback_, Run(DataTypeController::OK, _));
  bookmark_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  EXPECT_EQ(DataTypeController::RUNNING, bookmark_dtc_->state());
}

TEST_F(BookmarkDataTypeControllerTest, StartBookmarkModelNotReady) {
  SetStartExpectations();
  EXPECT_CALL(bookmark_model_, IsLoaded()).WillRepeatedly(Return(false));
  SetAssociateExpectations();

  EXPECT_CALL(start_callback_, Run(DataTypeController::OK, _));
  bookmark_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  EXPECT_EQ(DataTypeController::MODEL_STARTING, bookmark_dtc_->state());

  // Send the notification that the bookmark model has started.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_BOOKMARK_MODEL_LOADED,
      content::Source<Profile>(&profile_),
      content::NotificationService::NoDetails());
  EXPECT_EQ(DataTypeController::RUNNING, bookmark_dtc_->state());
}

TEST_F(BookmarkDataTypeControllerTest, StartFirstRun) {
  SetStartExpectations();
  SetAssociateExpectations();
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillRepeatedly(DoAll(SetArgumentPointee<0>(false), Return(true)));
  EXPECT_CALL(start_callback_, Run(DataTypeController::OK_FIRST_RUN, _));
  bookmark_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
}

TEST_F(BookmarkDataTypeControllerTest, StartBusy) {
  SetStartExpectations();
  EXPECT_CALL(bookmark_model_, IsLoaded()).WillRepeatedly(Return(false));

  EXPECT_CALL(start_callback_, Run(DataTypeController::BUSY, _));
  bookmark_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  bookmark_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
}

TEST_F(BookmarkDataTypeControllerTest, StartOk) {
  SetStartExpectations();
  SetAssociateExpectations();
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillRepeatedly(DoAll(SetArgumentPointee<0>(true), Return(true)));

  EXPECT_CALL(start_callback_, Run(DataTypeController::OK, _));
  bookmark_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
}

TEST_F(BookmarkDataTypeControllerTest, StartAssociationFailed) {
  SetStartExpectations();
  // Set up association to fail.
  EXPECT_CALL(*profile_sync_factory_, CreateBookmarkSyncComponents(_, _));
  EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary()).
      WillRepeatedly(Return(true));
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillRepeatedly(DoAll(SetArgumentPointee<0>(true), Return(true)));
  EXPECT_CALL(*model_associator_, AssociateModels(_)).
      WillRepeatedly(DoAll(browser_sync::SetSyncError(syncable::BOOKMARKS),
                           Return(false)));

  EXPECT_CALL(start_callback_, Run(DataTypeController::ASSOCIATION_FAILED, _));
  bookmark_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  EXPECT_EQ(DataTypeController::DISABLED, bookmark_dtc_->state());
}

TEST_F(BookmarkDataTypeControllerTest,
       StartAssociationTriggersUnrecoverableError) {
  SetStartExpectations();
  // Set up association to fail with an unrecoverable error.
  EXPECT_CALL(*profile_sync_factory_, CreateBookmarkSyncComponents(_, _));
  EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary()).
      WillRepeatedly(Return(true));
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillRepeatedly(DoAll(SetArgumentPointee<0>(false), Return(false)));
  EXPECT_CALL(start_callback_, Run(DataTypeController::UNRECOVERABLE_ERROR, _));
  bookmark_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  EXPECT_EQ(DataTypeController::NOT_RUNNING, bookmark_dtc_->state());
}

TEST_F(BookmarkDataTypeControllerTest, StartAborted) {
  SetStartExpectations();
  EXPECT_CALL(bookmark_model_, IsLoaded()).WillRepeatedly(Return(false));

  EXPECT_CALL(start_callback_, Run(DataTypeController::ABORTED, _));
  bookmark_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  bookmark_dtc_->Stop();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, bookmark_dtc_->state());
}

TEST_F(BookmarkDataTypeControllerTest, Stop) {
  SetStartExpectations();
  SetAssociateExpectations();
  SetStopExpectations();

  EXPECT_EQ(DataTypeController::NOT_RUNNING, bookmark_dtc_->state());

  EXPECT_CALL(start_callback_, Run(DataTypeController::OK, _));
  bookmark_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  EXPECT_EQ(DataTypeController::RUNNING, bookmark_dtc_->state());
  bookmark_dtc_->Stop();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, bookmark_dtc_->state());
}

TEST_F(BookmarkDataTypeControllerTest, OnUnrecoverableError) {
  SetStartExpectations();
  SetAssociateExpectations();
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillRepeatedly(DoAll(SetArgumentPointee<0>(true), Return(true)));
  EXPECT_CALL(service_, OnUnrecoverableError(_,_)).
      WillOnce(InvokeWithoutArgs(bookmark_dtc_.get(),
                                 &BookmarkDataTypeController::Stop));
  SetStopExpectations();

  EXPECT_CALL(start_callback_, Run(DataTypeController::OK, _));
  bookmark_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  // This should cause bookmark_dtc_->Stop() to be called.
  bookmark_dtc_->OnUnrecoverableError(FROM_HERE, "Test");
  PumpLoop();
}
