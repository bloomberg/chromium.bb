// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
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
#include "sync/api/sync_error.h"
#include "testing/gmock/include/gmock/gmock.h"

using browser_sync::BookmarkDataTypeController;
using browser_sync::ChangeProcessorMock;
using browser_sync::DataTypeController;
using browser_sync::ModelAssociatorMock;
using browser_sync::StartCallbackMock;
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

class HistoryMock : public HistoryService {
 public:
  HistoryMock() {}
  MOCK_METHOD0(BackendLoaded, bool(void));

 protected:
  virtual ~HistoryMock() {}
};


class SyncBookmarkDataTypeControllerTest : public testing::Test {
 public:
  SyncBookmarkDataTypeControllerTest()
      : ui_thread_(BrowserThread::UI, &message_loop_) {}

  virtual void SetUp() {
    model_associator_ = new ModelAssociatorMock();
    change_processor_ = new ChangeProcessorMock();
    history_service_ = new HistoryMock();
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
        WillRepeatedly(Return(&bookmark_model_));
    EXPECT_CALL(profile_, GetHistoryService(_)).
        WillRepeatedly(Return(history_service_.get()));
    EXPECT_CALL(bookmark_model_, IsLoaded()).WillRepeatedly(Return(true));
    EXPECT_CALL(*history_service_,
                BackendLoaded()).WillRepeatedly(Return(true));
  }

  void SetAssociateExpectations() {
    EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary()).
        WillRepeatedly(Return(true));
    EXPECT_CALL(*profile_sync_factory_, CreateBookmarkSyncComponents(_, _));
    EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
        WillRepeatedly(DoAll(SetArgumentPointee<0>(true), Return(true)));
    EXPECT_CALL(*model_associator_, AssociateModels()).
        WillRepeatedly(Return(SyncError()));
    EXPECT_CALL(service_, ActivateDataType(_, _, _));
  }

  void SetStopExpectations() {
    EXPECT_CALL(service_, DeactivateDataType(_));
    EXPECT_CALL(*model_associator_, DisassociateModels()).
                WillOnce(Return(SyncError()));
  }

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  scoped_refptr<BookmarkDataTypeController> bookmark_dtc_;
  scoped_ptr<ProfileSyncComponentsFactoryMock> profile_sync_factory_;
  ProfileMock profile_;
  BookmarkModelMock bookmark_model_;
  scoped_refptr<HistoryMock> history_service_;
  ProfileSyncServiceMock service_;
  ModelAssociatorMock* model_associator_;
  ChangeProcessorMock* change_processor_;
  StartCallbackMock start_callback_;

  void PumpLoop() {
    message_loop_.RunAllPending();
  }
};

TEST_F(SyncBookmarkDataTypeControllerTest, StartDependentsReady) {
  SetStartExpectations();
  SetAssociateExpectations();

  EXPECT_EQ(DataTypeController::NOT_RUNNING, bookmark_dtc_->state());

  EXPECT_CALL(start_callback_, Run(DataTypeController::OK, _));
  bookmark_dtc_->Start(
      base::Bind(&StartCallbackMock::Run, base::Unretained(&start_callback_)));
  EXPECT_EQ(DataTypeController::RUNNING, bookmark_dtc_->state());
}

TEST_F(SyncBookmarkDataTypeControllerTest, StartBookmarkModelNotReady) {
  SetStartExpectations();
  EXPECT_CALL(bookmark_model_, IsLoaded()).WillRepeatedly(Return(false));
  SetAssociateExpectations();

  EXPECT_CALL(start_callback_, Run(DataTypeController::OK, _));
  bookmark_dtc_->Start(
      base::Bind(&StartCallbackMock::Run, base::Unretained(&start_callback_)));
  EXPECT_EQ(DataTypeController::MODEL_STARTING, bookmark_dtc_->state());
  testing::Mock::VerifyAndClearExpectations(&bookmark_model_);
  EXPECT_CALL(bookmark_model_, IsLoaded()).WillRepeatedly(Return(true));

  // Send the notification that the bookmark model has started.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_BOOKMARK_MODEL_LOADED,
      content::Source<Profile>(&profile_),
      content::NotificationService::NoDetails());
  EXPECT_EQ(DataTypeController::RUNNING, bookmark_dtc_->state());
}

TEST_F(SyncBookmarkDataTypeControllerTest, StartHistoryServiceNotReady) {
  SetStartExpectations();
  EXPECT_CALL(*history_service_,
              BackendLoaded()).WillRepeatedly(Return(false));
  SetAssociateExpectations();

  EXPECT_CALL(start_callback_, Run(DataTypeController::OK, _));
  bookmark_dtc_->Start(
      base::Bind(&StartCallbackMock::Run, base::Unretained(&start_callback_)));
  EXPECT_EQ(DataTypeController::MODEL_STARTING, bookmark_dtc_->state());
  testing::Mock::VerifyAndClearExpectations(history_service_.get());
  EXPECT_CALL(*history_service_, BackendLoaded()).WillRepeatedly(Return(true));

  // Send the notification that the history service has finished loading the db.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_HISTORY_LOADED,
      content::Source<Profile>(&profile_),
      content::NotificationService::NoDetails());
  EXPECT_EQ(DataTypeController::RUNNING, bookmark_dtc_->state());
}

TEST_F(SyncBookmarkDataTypeControllerTest, StartFirstRun) {
  SetStartExpectations();
  SetAssociateExpectations();
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillRepeatedly(DoAll(SetArgumentPointee<0>(false), Return(true)));
  EXPECT_CALL(start_callback_, Run(DataTypeController::OK_FIRST_RUN, _));
  bookmark_dtc_->Start(
      base::Bind(&StartCallbackMock::Run, base::Unretained(&start_callback_)));
}

TEST_F(SyncBookmarkDataTypeControllerTest, StartBusy) {
  SetStartExpectations();
  EXPECT_CALL(bookmark_model_, IsLoaded()).WillRepeatedly(Return(false));
  EXPECT_CALL(*history_service_, BackendLoaded()).WillRepeatedly(Return(false));

  EXPECT_CALL(start_callback_, Run(DataTypeController::BUSY, _));
  bookmark_dtc_->Start(
      base::Bind(&StartCallbackMock::Run, base::Unretained(&start_callback_)));
  bookmark_dtc_->Start(
      base::Bind(&StartCallbackMock::Run, base::Unretained(&start_callback_)));
}

TEST_F(SyncBookmarkDataTypeControllerTest, StartOk) {
  SetStartExpectations();
  SetAssociateExpectations();
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillRepeatedly(DoAll(SetArgumentPointee<0>(true), Return(true)));

  EXPECT_CALL(start_callback_, Run(DataTypeController::OK, _));
  bookmark_dtc_->Start(
      base::Bind(&StartCallbackMock::Run, base::Unretained(&start_callback_)));
}

TEST_F(SyncBookmarkDataTypeControllerTest, StartAssociationFailed) {
  SetStartExpectations();
  // Set up association to fail.
  EXPECT_CALL(*profile_sync_factory_, CreateBookmarkSyncComponents(_, _));
  EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary()).
      WillRepeatedly(Return(true));
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillRepeatedly(DoAll(SetArgumentPointee<0>(true), Return(true)));
  EXPECT_CALL(*model_associator_, AssociateModels()).
      WillRepeatedly(Return(SyncError(FROM_HERE,
                                     "error",
                                     syncable::BOOKMARKS)));

  EXPECT_CALL(start_callback_,
              Run(DataTypeController::ASSOCIATION_FAILED, _));
  bookmark_dtc_->Start(
      base::Bind(&StartCallbackMock::Run, base::Unretained(&start_callback_)));
  EXPECT_EQ(DataTypeController::DISABLED, bookmark_dtc_->state());
}

TEST_F(SyncBookmarkDataTypeControllerTest,
       StartAssociationTriggersUnrecoverableError) {
  SetStartExpectations();
  // Set up association to fail with an unrecoverable error.
  EXPECT_CALL(*profile_sync_factory_, CreateBookmarkSyncComponents(_, _));
  EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary()).
      WillRepeatedly(Return(true));
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillRepeatedly(DoAll(SetArgumentPointee<0>(false), Return(false)));
  EXPECT_CALL(start_callback_,
              Run(DataTypeController::UNRECOVERABLE_ERROR, _));
  bookmark_dtc_->Start(
      base::Bind(&StartCallbackMock::Run, base::Unretained(&start_callback_)));
  EXPECT_EQ(DataTypeController::NOT_RUNNING, bookmark_dtc_->state());
}

TEST_F(SyncBookmarkDataTypeControllerTest, StartAborted) {
  SetStartExpectations();
  EXPECT_CALL(bookmark_model_, IsLoaded()).WillRepeatedly(Return(false));
  EXPECT_CALL(*history_service_, BackendLoaded()).WillRepeatedly(Return(false));

  EXPECT_CALL(start_callback_, Run(DataTypeController::ABORTED, _));
  bookmark_dtc_->Start(
      base::Bind(&StartCallbackMock::Run, base::Unretained(&start_callback_)));
  bookmark_dtc_->Stop();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, bookmark_dtc_->state());
}

TEST_F(SyncBookmarkDataTypeControllerTest, Stop) {
  SetStartExpectations();
  SetAssociateExpectations();
  SetStopExpectations();

  EXPECT_EQ(DataTypeController::NOT_RUNNING, bookmark_dtc_->state());

  EXPECT_CALL(start_callback_, Run(DataTypeController::OK, _));
  bookmark_dtc_->Start(
      base::Bind(&StartCallbackMock::Run, base::Unretained(&start_callback_)));
  EXPECT_EQ(DataTypeController::RUNNING, bookmark_dtc_->state());
  bookmark_dtc_->Stop();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, bookmark_dtc_->state());
}

TEST_F(SyncBookmarkDataTypeControllerTest, OnUnrecoverableError) {
  SetStartExpectations();
  SetAssociateExpectations();
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillRepeatedly(DoAll(SetArgumentPointee<0>(true), Return(true)));
  EXPECT_CALL(service_, OnUnrecoverableError(_,_)).
      WillOnce(InvokeWithoutArgs(bookmark_dtc_.get(),
                                 &BookmarkDataTypeController::Stop));
  SetStopExpectations();

  EXPECT_CALL(start_callback_, Run(DataTypeController::OK, _));
  bookmark_dtc_->Start(
      base::Bind(&StartCallbackMock::Run, base::Unretained(&start_callback_)));
  // This should cause bookmark_dtc_->Stop() to be called.
  bookmark_dtc_->OnUnrecoverableError(FROM_HERE, "Test");
  PumpLoop();
}
