// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/callback.h"
#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/task.h"
#include "base/tracked_objects.h"
#include "chrome/browser/sync/glue/preference_data_type_controller.h"
#include "chrome/browser/sync/glue/change_processor_mock.h"
#include "chrome/browser/sync/glue/model_associator_mock.h"
#include "chrome/browser/sync/profile_sync_factory_mock.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "content/browser/browser_thread.h"

using browser_sync::PreferenceDataTypeController;
using browser_sync::ChangeProcessorMock;
using browser_sync::DataTypeController;
using browser_sync::ModelAssociatorMock;
using testing::_;
using testing::DoAll;
using testing::InvokeWithoutArgs;
using testing::Return;
using testing::SetArgumentPointee;

class StartCallback {
 public:
  MOCK_METHOD1(Run, void(DataTypeController::StartResult result));
};

class PreferenceDataTypeControllerTest : public testing::Test {
 public:
  PreferenceDataTypeControllerTest()
      : ui_thread_(BrowserThread::UI, &message_loop_) {}

  virtual void SetUp() {
    profile_sync_factory_.reset(new ProfileSyncFactoryMock());
    preference_dtc_ =
        new PreferenceDataTypeController(profile_sync_factory_.get(),
                                         &service_);
  }

 protected:
  void SetStartExpectations() {
    model_associator_ = new ModelAssociatorMock();
    change_processor_ = new ChangeProcessorMock();
    EXPECT_CALL(*profile_sync_factory_, CreatePreferenceSyncComponents(_, _)).
        WillOnce(Return(ProfileSyncFactory::SyncComponents(model_associator_,
                                                           change_processor_)));
  }

  void SetAssociateExpectations() {
    EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
        WillRepeatedly(DoAll(SetArgumentPointee<0>(true), Return(true)));
    EXPECT_CALL(*model_associator_, AssociateModels()).
        WillRepeatedly(Return(true));
  }

  void SetActivateExpectations() {
    EXPECT_CALL(service_, ActivateDataType(_, _));
  }

  void SetStopExpectations() {
    EXPECT_CALL(service_, DeactivateDataType(_, _));
    EXPECT_CALL(*model_associator_, DisassociateModels());
  }

  MessageLoopForUI message_loop_;
  BrowserThread ui_thread_;
  scoped_refptr<PreferenceDataTypeController> preference_dtc_;
  scoped_ptr<ProfileSyncFactoryMock> profile_sync_factory_;
  ProfileSyncServiceMock service_;
  ModelAssociatorMock* model_associator_;
  ChangeProcessorMock* change_processor_;
  StartCallback start_callback_;
};

TEST_F(PreferenceDataTypeControllerTest, Start) {
  SetStartExpectations();
  SetAssociateExpectations();
  SetActivateExpectations();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, preference_dtc_->state());
  EXPECT_CALL(start_callback_, Run(DataTypeController::OK));
  preference_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  EXPECT_EQ(DataTypeController::RUNNING, preference_dtc_->state());
}

TEST_F(PreferenceDataTypeControllerTest, StartFirstRun) {
  SetStartExpectations();
  SetAssociateExpectations();
  SetActivateExpectations();
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillRepeatedly(DoAll(SetArgumentPointee<0>(false), Return(true)));
  EXPECT_CALL(start_callback_, Run(DataTypeController::OK_FIRST_RUN));
  preference_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
}

TEST_F(PreferenceDataTypeControllerTest, StartOk) {
  SetStartExpectations();
  SetAssociateExpectations();
  SetActivateExpectations();
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillRepeatedly(DoAll(SetArgumentPointee<0>(true), Return(true)));

  EXPECT_CALL(start_callback_, Run(DataTypeController::OK));
  preference_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
}

TEST_F(PreferenceDataTypeControllerTest, StartAssociationFailed) {
  SetStartExpectations();
  SetAssociateExpectations();
  EXPECT_CALL(*model_associator_, AssociateModels()).
      WillRepeatedly(Return(false));

  EXPECT_CALL(start_callback_, Run(DataTypeController::ASSOCIATION_FAILED));
  preference_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  EXPECT_EQ(DataTypeController::NOT_RUNNING, preference_dtc_->state());
}

TEST_F(PreferenceDataTypeControllerTest,
       StartAssociationTriggersUnrecoverableError) {
  SetStartExpectations();
  // Set up association to fail with an unrecoverable error.
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillRepeatedly(DoAll(SetArgumentPointee<0>(false), Return(false)));
  EXPECT_CALL(start_callback_, Run(DataTypeController::UNRECOVERABLE_ERROR));
  preference_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  EXPECT_EQ(DataTypeController::NOT_RUNNING, preference_dtc_->state());
}

TEST_F(PreferenceDataTypeControllerTest, Stop) {
  SetStartExpectations();
  SetAssociateExpectations();
  SetActivateExpectations();
  SetStopExpectations();

  EXPECT_EQ(DataTypeController::NOT_RUNNING, preference_dtc_->state());

  EXPECT_CALL(start_callback_, Run(DataTypeController::OK));
  preference_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  EXPECT_EQ(DataTypeController::RUNNING, preference_dtc_->state());
  preference_dtc_->Stop();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, preference_dtc_->state());
}

TEST_F(PreferenceDataTypeControllerTest, OnUnrecoverableError) {
  SetStartExpectations();
  SetAssociateExpectations();
  SetActivateExpectations();
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillRepeatedly(DoAll(SetArgumentPointee<0>(true), Return(true)));
  EXPECT_CALL(service_, OnUnrecoverableError(_,_)).
      WillOnce(InvokeWithoutArgs(preference_dtc_.get(),
                                 &PreferenceDataTypeController::Stop));
  SetStopExpectations();

  EXPECT_CALL(start_callback_, Run(DataTypeController::OK));
  preference_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  // This should cause preference_dtc_->Stop() to be called.
  preference_dtc_->OnUnrecoverableError(FROM_HERE, "Test");
}
