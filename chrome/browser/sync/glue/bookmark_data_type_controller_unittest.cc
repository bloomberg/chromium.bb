// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/bookmark_data_type_controller.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/bookmarks/bookmark_model.h"
#include "chrome/browser/bookmarks/bookmark_model_factory.h"
#include "chrome/browser/history/history_service.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/refcounted_profile_keyed_service.h"
#include "chrome/browser/sync/glue/change_processor_mock.h"
#include "chrome/browser/sync/glue/data_type_controller_mock.h"
#include "chrome/browser/sync/glue/model_associator_mock.h"
#include "chrome/browser/sync/profile_sync_components_factory_mock.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/profile_mock.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/test_browser_thread.h"
#include "sync/api/sync_error.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using browser_sync::BookmarkDataTypeController;
using browser_sync::ChangeProcessorMock;
using browser_sync::DataTypeController;
using browser_sync::ModelAssociatorMock;
using browser_sync::ModelLoadCallbackMock;
using browser_sync::StartCallbackMock;
using content::BrowserThread;
using testing::_;
using testing::DoAll;
using testing::InvokeWithoutArgs;
using testing::Return;
using testing::SetArgumentPointee;

namespace {

class HistoryMock : public HistoryService {
 public:
  explicit HistoryMock(Profile* profile) : HistoryService(profile) {}
  MOCK_METHOD0(BackendLoaded, bool(void));

 protected:
  virtual ~HistoryMock() {}
};

ProfileKeyedService* BuildBookmarkModel(Profile* profile) {
  BookmarkModel* bookmark_model = new BookmarkModel(profile);
  bookmark_model->Load();
  return bookmark_model;
}

ProfileKeyedService* BuildBookmarkModelWithoutLoading(Profile* profile) {
  return new BookmarkModel(profile);
}

ProfileKeyedService* BuildHistoryService(Profile* profile) {
  return new HistoryMock(profile);
}

}  // namespace

class SyncBookmarkDataTypeControllerTest : public testing::Test {
 public:
  SyncBookmarkDataTypeControllerTest()
      : ui_thread_(BrowserThread::UI, &message_loop_) {}

  virtual void SetUp() {
    model_associator_ = new ModelAssociatorMock();
    change_processor_ = new ChangeProcessorMock();
    history_service_ = static_cast<HistoryMock*>(
        HistoryServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            &profile_, BuildHistoryService));
    profile_sync_factory_.reset(
        new ProfileSyncComponentsFactoryMock(model_associator_,
                                             change_processor_));
    bookmark_dtc_ = new BookmarkDataTypeController(profile_sync_factory_.get(),
                                                   &profile_,
                                                   &service_);
  }

 protected:
  enum BookmarkLoadPolicy {
    DONT_LOAD_MODEL,
    LOAD_MODEL,
  };

  void CreateBookmarkModel(BookmarkLoadPolicy bookmark_load_policy) {
    if (bookmark_load_policy == LOAD_MODEL) {
      bookmark_model_ = static_cast<BookmarkModel*>(
          BookmarkModelFactory::GetInstance()->SetTestingFactoryAndUse(
              &profile_, BuildBookmarkModel));
      ui_test_utils::WaitForBookmarkModelToLoad(bookmark_model_);
    } else {
      bookmark_model_ = static_cast<BookmarkModel*>(
          BookmarkModelFactory::GetInstance()->SetTestingFactoryAndUse(
              &profile_, BuildBookmarkModelWithoutLoading));
    }
  }

  void SetStartExpectations() {
    EXPECT_CALL(*history_service_,
                BackendLoaded()).WillRepeatedly(Return(true));
    EXPECT_CALL(model_load_callback_, Run(_, _));
  }

  void SetAssociateExpectations() {
    EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary()).
        WillRepeatedly(Return(true));
    EXPECT_CALL(*profile_sync_factory_, CreateBookmarkSyncComponents(_, _));
    EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
        WillRepeatedly(DoAll(SetArgumentPointee<0>(true), Return(true)));
    EXPECT_CALL(*model_associator_, AssociateModels(_, _)).
        WillRepeatedly(Return(syncer::SyncError()));
    EXPECT_CALL(service_, ActivateDataType(_, _, _));
  }

  void SetStopExpectations() {
    EXPECT_CALL(service_, DeactivateDataType(_));
    EXPECT_CALL(*model_associator_, DisassociateModels()).
                WillOnce(Return(syncer::SyncError()));
  }

  void Start() {
    bookmark_dtc_->LoadModels(
        base::Bind(&ModelLoadCallbackMock::Run,
                   base::Unretained(&model_load_callback_)));
    bookmark_dtc_->StartAssociating(
        base::Bind(&StartCallbackMock::Run,
                   base::Unretained(&start_callback_)));
  }

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  scoped_refptr<BookmarkDataTypeController> bookmark_dtc_;
  scoped_ptr<ProfileSyncComponentsFactoryMock> profile_sync_factory_;
  ProfileMock profile_;
  BookmarkModel* bookmark_model_;
  HistoryMock* history_service_;
  ProfileSyncServiceMock service_;
  ModelAssociatorMock* model_associator_;
  ChangeProcessorMock* change_processor_;
  StartCallbackMock start_callback_;
  ModelLoadCallbackMock model_load_callback_;
};

TEST_F(SyncBookmarkDataTypeControllerTest, StartDependentsReady) {
  CreateBookmarkModel(LOAD_MODEL);
  SetStartExpectations();
  SetAssociateExpectations();

  EXPECT_EQ(DataTypeController::NOT_RUNNING, bookmark_dtc_->state());

  EXPECT_CALL(start_callback_, Run(DataTypeController::OK, _, _));
  Start();
  EXPECT_EQ(DataTypeController::RUNNING, bookmark_dtc_->state());
}

TEST_F(SyncBookmarkDataTypeControllerTest, StartBookmarkModelNotReady) {
  CreateBookmarkModel(DONT_LOAD_MODEL);
  SetStartExpectations();
  SetAssociateExpectations();

  EXPECT_CALL(start_callback_, Run(DataTypeController::OK, _, _));
  bookmark_dtc_->LoadModels(
      base::Bind(&ModelLoadCallbackMock::Run,
                 base::Unretained(&model_load_callback_)));
  EXPECT_EQ(DataTypeController::MODEL_STARTING, bookmark_dtc_->state());

  bookmark_model_->Load();
  ui_test_utils::WaitForBookmarkModelToLoad(bookmark_model_);
  EXPECT_EQ(DataTypeController::MODEL_LOADED, bookmark_dtc_->state());

  bookmark_dtc_->StartAssociating(
      base::Bind(&StartCallbackMock::Run,
                 base::Unretained(&start_callback_)));

  EXPECT_EQ(DataTypeController::RUNNING, bookmark_dtc_->state());
}

TEST_F(SyncBookmarkDataTypeControllerTest, StartHistoryServiceNotReady) {
  CreateBookmarkModel(LOAD_MODEL);
  SetStartExpectations();
  EXPECT_CALL(*history_service_,
              BackendLoaded()).WillRepeatedly(Return(false));

  bookmark_dtc_->LoadModels(
      base::Bind(&ModelLoadCallbackMock::Run,
                 base::Unretained(&model_load_callback_)));

  EXPECT_EQ(DataTypeController::MODEL_STARTING, bookmark_dtc_->state());
  testing::Mock::VerifyAndClearExpectations(history_service_);
  EXPECT_CALL(*history_service_, BackendLoaded()).WillRepeatedly(Return(true));

  // Send the notification that the history service has finished loading the db.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_HISTORY_LOADED,
      content::Source<Profile>(&profile_),
      content::NotificationService::NoDetails());
  EXPECT_EQ(DataTypeController::MODEL_LOADED, bookmark_dtc_->state());
}

TEST_F(SyncBookmarkDataTypeControllerTest, StartFirstRun) {
  CreateBookmarkModel(LOAD_MODEL);
  SetStartExpectations();
  SetAssociateExpectations();
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillRepeatedly(DoAll(SetArgumentPointee<0>(false), Return(true)));
  EXPECT_CALL(start_callback_, Run(DataTypeController::OK_FIRST_RUN, _, _));
  Start();
}

TEST_F(SyncBookmarkDataTypeControllerTest, StartBusy) {
  CreateBookmarkModel(LOAD_MODEL);
  EXPECT_CALL(*history_service_, BackendLoaded()).WillRepeatedly(Return(false));

  EXPECT_CALL(model_load_callback_, Run(_, _));
  bookmark_dtc_->LoadModels(
      base::Bind(&ModelLoadCallbackMock::Run,
                 base::Unretained(&model_load_callback_)));
  bookmark_dtc_->LoadModels(
      base::Bind(&ModelLoadCallbackMock::Run,
                 base::Unretained(&model_load_callback_)));
}

TEST_F(SyncBookmarkDataTypeControllerTest, StartOk) {
  CreateBookmarkModel(LOAD_MODEL);
  SetStartExpectations();
  SetAssociateExpectations();
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillRepeatedly(DoAll(SetArgumentPointee<0>(true), Return(true)));

  EXPECT_CALL(start_callback_, Run(DataTypeController::OK, _, _));
  Start();
}

TEST_F(SyncBookmarkDataTypeControllerTest, StartAssociationFailed) {
  CreateBookmarkModel(LOAD_MODEL);
  SetStartExpectations();
  // Set up association to fail.
  EXPECT_CALL(*profile_sync_factory_, CreateBookmarkSyncComponents(_, _));
  EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary()).
      WillRepeatedly(Return(true));
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillRepeatedly(DoAll(SetArgumentPointee<0>(true), Return(true)));
  EXPECT_CALL(*model_associator_, AssociateModels(_, _)).
      WillRepeatedly(Return(syncer::SyncError(FROM_HERE,
                                     "error",
                                     syncer::BOOKMARKS)));

  EXPECT_CALL(start_callback_,
              Run(DataTypeController::ASSOCIATION_FAILED, _, _));
  Start();
  EXPECT_EQ(DataTypeController::DISABLED, bookmark_dtc_->state());
}

TEST_F(SyncBookmarkDataTypeControllerTest,
       StartAssociationTriggersUnrecoverableError) {
  CreateBookmarkModel(LOAD_MODEL);
  SetStartExpectations();
  // Set up association to fail with an unrecoverable error.
  EXPECT_CALL(*profile_sync_factory_, CreateBookmarkSyncComponents(_, _));
  EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary()).
      WillRepeatedly(Return(true));
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillRepeatedly(DoAll(SetArgumentPointee<0>(false), Return(false)));
  EXPECT_CALL(start_callback_,
              Run(DataTypeController::UNRECOVERABLE_ERROR, _, _));
  Start();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, bookmark_dtc_->state());
}

TEST_F(SyncBookmarkDataTypeControllerTest, StartAborted) {
  CreateBookmarkModel(LOAD_MODEL);
  EXPECT_CALL(*history_service_, BackendLoaded()).WillRepeatedly(Return(false));

  EXPECT_CALL(model_load_callback_, Run(_, _));
  bookmark_dtc_->LoadModels(
      base::Bind(&ModelLoadCallbackMock::Run,
                 base::Unretained(&model_load_callback_)));

  bookmark_dtc_->Stop();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, bookmark_dtc_->state());
}

TEST_F(SyncBookmarkDataTypeControllerTest, Stop) {
  CreateBookmarkModel(LOAD_MODEL);
  SetStartExpectations();
  SetAssociateExpectations();
  SetStopExpectations();

  EXPECT_EQ(DataTypeController::NOT_RUNNING, bookmark_dtc_->state());

  EXPECT_CALL(start_callback_, Run(DataTypeController::OK, _, _));
  Start();
  EXPECT_EQ(DataTypeController::RUNNING, bookmark_dtc_->state());
  bookmark_dtc_->Stop();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, bookmark_dtc_->state());
}

TEST_F(SyncBookmarkDataTypeControllerTest, OnSingleDatatypeUnrecoverableError) {
  CreateBookmarkModel(LOAD_MODEL);
  SetStartExpectations();
  SetAssociateExpectations();
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillRepeatedly(DoAll(SetArgumentPointee<0>(true), Return(true)));
  EXPECT_CALL(service_, DisableBrokenDatatype(_,_,_)).
      WillOnce(InvokeWithoutArgs(bookmark_dtc_.get(),
                                 &BookmarkDataTypeController::Stop));
  SetStopExpectations();

  EXPECT_CALL(start_callback_, Run(DataTypeController::OK, _, _));
  Start();
  // This should cause bookmark_dtc_->Stop() to be called.
  bookmark_dtc_->OnSingleDatatypeUnrecoverableError(FROM_HERE, "Test");
  base::RunLoop().RunUntilIdle();
}
