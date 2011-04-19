// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "base/tracked_objects.h"
#include "chrome/browser/sync/glue/change_processor_mock.h"
#include "chrome/browser/sync/glue/frontend_data_type_controller.h"
#include "chrome/browser/sync/glue/frontend_data_type_controller_mock.h"
#include "chrome/browser/sync/glue/model_associator_mock.h"
#include "chrome/browser/sync/profile_sync_factory_mock.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/test/profile_mock.h"
#include "content/browser/browser_thread.h"

using browser_sync::ChangeProcessorMock;
using browser_sync::DataTypeController;
using browser_sync::FrontendDataTypeController;
using browser_sync::FrontendDataTypeControllerMock;
using browser_sync::ModelAssociatorMock;
using testing::_;
using testing::DoAll;
using testing::InvokeWithoutArgs;
using testing::Return;
using testing::SetArgumentPointee;
using testing::StrictMock;

class StartCallback {
 public:
  MOCK_METHOD2(Run, void(DataTypeController::StartResult result,
                         const tracked_objects::Location& from_here));
};

class FrontendDataTypeControllerFake : public FrontendDataTypeController {
 public:
  FrontendDataTypeControllerFake(
      ProfileSyncFactory* profile_sync_factory,
      Profile* profile,
      ProfileSyncService* sync_service,
      FrontendDataTypeControllerMock* mock)
      : FrontendDataTypeController(profile_sync_factory,
                                   profile,
                                   sync_service),
        mock_(mock) {}
  virtual syncable::ModelType type() const { return syncable::BOOKMARKS; }

 private:
  virtual void CreateSyncComponents() {
    ProfileSyncFactory::SyncComponents sync_components =
        profile_sync_factory_->
            CreateBookmarkSyncComponents(sync_service_, this);
    model_associator_.reset(sync_components.model_associator);
    change_processor_.reset(sync_components.change_processor);
  }

  // We mock the following methods because their default implementations do
  // nothing, but we still want to make sure they're called appropriately.
  virtual bool StartModels() {
    return mock_->StartModels();
  }
  virtual void CleanupState() {
    mock_->CleanupState();
  }
  virtual void RecordUnrecoverableError(
      const tracked_objects::Location& from_here,
      const std::string& message) {
    mock_->RecordUnrecoverableError(from_here, message);
  }
  virtual void RecordAssociationTime(base::TimeDelta time) {
    mock_->RecordAssociationTime(time);
  }
  virtual void RecordStartFailure(DataTypeController::StartResult result) {
    mock_->RecordStartFailure(result);
  }
 private:
  FrontendDataTypeControllerMock* mock_;
};

class FrontendDataTypeControllerTest : public testing::Test {
 public:
  FrontendDataTypeControllerTest()
      : ui_thread_(BrowserThread::UI, &message_loop_) {}

  virtual void SetUp() {
    profile_sync_factory_.reset(new ProfileSyncFactoryMock());
    dtc_mock_ = new StrictMock<FrontendDataTypeControllerMock>();
    frontend_dtc_ =
        new FrontendDataTypeControllerFake(profile_sync_factory_.get(),
                                           &profile_,
                                           &service_,
                                           dtc_mock_.get());
  }

 protected:
  void SetStartExpectations() {
    EXPECT_CALL(*dtc_mock_, StartModels()).WillOnce(Return(true));
    model_associator_ = new ModelAssociatorMock();
    change_processor_ = new ChangeProcessorMock();
    EXPECT_CALL(*profile_sync_factory_, CreateBookmarkSyncComponents(_, _)).
        WillOnce(Return(ProfileSyncFactory::SyncComponents(model_associator_,
                                                           change_processor_)));
  }

  void SetAssociateExpectations() {
    EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary()).
        WillOnce(Return(true));
    EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
        WillOnce(DoAll(SetArgumentPointee<0>(true), Return(true)));
    EXPECT_CALL(*model_associator_, AssociateModels()).
        WillOnce(Return(true));
    EXPECT_CALL(*dtc_mock_, RecordAssociationTime(_));
  }

  void SetActivateExpectations(DataTypeController::StartResult result) {
    EXPECT_CALL(service_, ActivateDataType(_, _));
    EXPECT_CALL(start_callback_, Run(result,_));
  }

  void SetStopExpectations() {
    EXPECT_CALL(*dtc_mock_, CleanupState());
    EXPECT_CALL(service_, DeactivateDataType(_, _));
    EXPECT_CALL(*model_associator_, DisassociateModels());
  }

  void SetStartFailExpectations(DataTypeController::StartResult result) {
    EXPECT_CALL(*dtc_mock_, CleanupState());
    EXPECT_CALL(*dtc_mock_, RecordStartFailure(result));
    EXPECT_CALL(start_callback_, Run(result,_));
  }

  MessageLoopForUI message_loop_;
  BrowserThread ui_thread_;
  scoped_refptr<FrontendDataTypeControllerFake> frontend_dtc_;
  scoped_ptr<ProfileSyncFactoryMock> profile_sync_factory_;
  scoped_refptr<FrontendDataTypeControllerMock> dtc_mock_;
  ProfileMock profile_;
  ProfileSyncServiceMock service_;
  ModelAssociatorMock* model_associator_;
  ChangeProcessorMock* change_processor_;
  StartCallback start_callback_;
};

TEST_F(FrontendDataTypeControllerTest, StartOk) {
  SetStartExpectations();
  SetAssociateExpectations();
  SetActivateExpectations(DataTypeController::OK);
  EXPECT_EQ(DataTypeController::NOT_RUNNING, frontend_dtc_->state());
  frontend_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  EXPECT_EQ(DataTypeController::RUNNING, frontend_dtc_->state());
}

TEST_F(FrontendDataTypeControllerTest, StartFirstRun) {
  SetStartExpectations();
  EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary()).
      WillOnce(Return(true));
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(false), Return(true)));
  EXPECT_CALL(*model_associator_, AssociateModels()).
      WillOnce(Return(true));
  EXPECT_CALL(*dtc_mock_, RecordAssociationTime(_));
  SetActivateExpectations(DataTypeController::OK_FIRST_RUN);
  EXPECT_EQ(DataTypeController::NOT_RUNNING, frontend_dtc_->state());
  frontend_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  EXPECT_EQ(DataTypeController::RUNNING, frontend_dtc_->state());
}

TEST_F(FrontendDataTypeControllerTest, StartAssociationFailed) {
  SetStartExpectations();
  EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary()).
      WillOnce(Return(true));
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(true), Return(true)));
  EXPECT_CALL(*model_associator_, AssociateModels()).
      WillOnce(Return(false));
  EXPECT_CALL(*dtc_mock_, RecordAssociationTime(_));
  SetStartFailExpectations(DataTypeController::ASSOCIATION_FAILED);
  // Set up association to fail with an association failed error.
  EXPECT_EQ(DataTypeController::NOT_RUNNING, frontend_dtc_->state());
  frontend_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  EXPECT_EQ(DataTypeController::NOT_RUNNING, frontend_dtc_->state());
}

TEST_F(FrontendDataTypeControllerTest,
       StartAssociationTriggersUnrecoverableError) {
  SetStartExpectations();
  SetStartFailExpectations(DataTypeController::UNRECOVERABLE_ERROR);
  // Set up association to fail with an unrecoverable error.
  EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary()).
      WillRepeatedly(Return(true));
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillRepeatedly(DoAll(SetArgumentPointee<0>(false), Return(false)));
  EXPECT_EQ(DataTypeController::NOT_RUNNING, frontend_dtc_->state());
  frontend_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  EXPECT_EQ(DataTypeController::NOT_RUNNING, frontend_dtc_->state());
}

TEST_F(FrontendDataTypeControllerTest, StartAssociationCryptoNotReady) {
  SetStartExpectations();
  SetStartFailExpectations(DataTypeController::NEEDS_CRYPTO);
  // Set up association to fail with a NEEDS_CRYPTO error.
  EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary()).
      WillRepeatedly(Return(false));
  EXPECT_EQ(DataTypeController::NOT_RUNNING, frontend_dtc_->state());
  frontend_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  EXPECT_EQ(DataTypeController::NOT_RUNNING, frontend_dtc_->state());
}

TEST_F(FrontendDataTypeControllerTest, Stop) {
  SetStartExpectations();
  SetAssociateExpectations();
  SetActivateExpectations(DataTypeController::OK);
  SetStopExpectations();

  EXPECT_EQ(DataTypeController::NOT_RUNNING, frontend_dtc_->state());
  frontend_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  EXPECT_EQ(DataTypeController::RUNNING, frontend_dtc_->state());
  frontend_dtc_->Stop();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, frontend_dtc_->state());
}

TEST_F(FrontendDataTypeControllerTest, OnUnrecoverableError) {
  SetStartExpectations();
  SetAssociateExpectations();
  SetActivateExpectations(DataTypeController::OK);
  EXPECT_CALL(*dtc_mock_, RecordUnrecoverableError(_, "Test"));
  EXPECT_CALL(service_, OnUnrecoverableError(_,_)).
      WillOnce(InvokeWithoutArgs(frontend_dtc_.get(),
                                 &FrontendDataTypeController::Stop));
  SetStopExpectations();

  EXPECT_EQ(DataTypeController::NOT_RUNNING, frontend_dtc_->state());
  frontend_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  EXPECT_EQ(DataTypeController::RUNNING, frontend_dtc_->state());
  // This should cause frontend_dtc_->Stop() to be called.
  frontend_dtc_->OnUnrecoverableError(FROM_HERE, "Test");
  EXPECT_EQ(DataTypeController::NOT_RUNNING, frontend_dtc_->state());
}
