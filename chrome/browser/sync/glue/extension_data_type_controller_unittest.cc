// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/task.h"
#include "chrome/browser/sync/glue/change_processor_mock.h"
#include "chrome/browser/sync/glue/data_type_controller_mock.h"
#include "chrome/browser/sync/glue/extension_data_type_controller.h"
#include "chrome/browser/sync/glue/model_associator_mock.h"
#include "chrome/browser/sync/profile_sync_components_factory_mock.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/test/base/profile_mock.h"
#include "content/test/test_browser_thread.h"

using browser_sync::ExtensionDataTypeController;
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

class ExtensionDataTypeControllerTest : public testing::Test {
 public:
  ExtensionDataTypeControllerTest()
      : ui_thread_(BrowserThread::UI, &message_loop_) {}

  virtual void SetUp() {
    profile_sync_factory_.reset(new ProfileSyncComponentsFactoryMock());
    extension_dtc_ =
        new ExtensionDataTypeController(profile_sync_factory_.get(),
                                        &profile_, &service_);
  }

 protected:
  void SetStartExpectations() {
    model_associator_ = new ModelAssociatorMock();
    change_processor_ = new ChangeProcessorMock();
    EXPECT_CALL(*profile_sync_factory_, CreateExtensionSyncComponents(_, _)).
        WillOnce(Return(
            ProfileSyncComponentsFactory::SyncComponents(model_associator_,
                                               change_processor_)));
  }

  void SetAssociateExpectations() {
    EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary()).
        WillRepeatedly(Return(true));
    EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
        WillRepeatedly(DoAll(SetArgumentPointee<0>(true), Return(true)));
    EXPECT_CALL(*model_associator_, AssociateModels(_)).
        WillRepeatedly(Return(true));
  }

  void SetActivateExpectations() {
    EXPECT_CALL(service_, ActivateDataType(_, _, _));
  }

  void SetStopExpectations() {
    EXPECT_CALL(service_, DeactivateDataType(_));
    EXPECT_CALL(*model_associator_, DisassociateModels(_));
  }

  void PumpLoop() {
    message_loop_.RunAllPending();
  }

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  scoped_refptr<ExtensionDataTypeController> extension_dtc_;
  scoped_ptr<ProfileSyncComponentsFactoryMock> profile_sync_factory_;
  ProfileMock profile_;
  ProfileSyncServiceMock service_;
  ModelAssociatorMock* model_associator_;
  ChangeProcessorMock* change_processor_;
  StartCallback start_callback_;
};

TEST_F(ExtensionDataTypeControllerTest, Start) {
  SetStartExpectations();
  SetAssociateExpectations();
  SetActivateExpectations();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, extension_dtc_->state());
  EXPECT_CALL(start_callback_, Run(DataTypeController::OK, _));
  extension_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  EXPECT_EQ(DataTypeController::RUNNING, extension_dtc_->state());
}

TEST_F(ExtensionDataTypeControllerTest, StartFirstRun) {
  SetStartExpectations();
  SetAssociateExpectations();
  SetActivateExpectations();
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillRepeatedly(DoAll(SetArgumentPointee<0>(false), Return(true)));
  EXPECT_CALL(start_callback_, Run(DataTypeController::OK_FIRST_RUN, _));
  extension_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
}

TEST_F(ExtensionDataTypeControllerTest, StartOk) {
  SetStartExpectations();
  SetAssociateExpectations();
  SetActivateExpectations();
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillRepeatedly(DoAll(SetArgumentPointee<0>(true), Return(true)));

  EXPECT_CALL(start_callback_, Run(DataTypeController::OK, _));
  extension_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
}

TEST_F(ExtensionDataTypeControllerTest, StartAssociationFailed) {
  SetStartExpectations();
  SetAssociateExpectations();
  EXPECT_CALL(*model_associator_, AssociateModels(_)).
      WillRepeatedly(DoAll(browser_sync::SetSyncError(syncable::EXTENSIONS),
                           Return(false)));

  EXPECT_CALL(start_callback_, Run(DataTypeController::ASSOCIATION_FAILED, _));
  extension_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  EXPECT_EQ(DataTypeController::DISABLED, extension_dtc_->state());
}

TEST_F(ExtensionDataTypeControllerTest,
       StartAssociationTriggersUnrecoverableError) {
  SetStartExpectations();
  // Set up association to fail with an unrecoverable error.
  EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary()).
      WillRepeatedly(Return(true));
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillRepeatedly(DoAll(SetArgumentPointee<0>(false), Return(false)));
  EXPECT_CALL(start_callback_, Run(DataTypeController::UNRECOVERABLE_ERROR, _));
  extension_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  EXPECT_EQ(DataTypeController::NOT_RUNNING, extension_dtc_->state());
}

TEST_F(ExtensionDataTypeControllerTest, Stop) {
  SetStartExpectations();
  SetAssociateExpectations();
  SetActivateExpectations();
  SetStopExpectations();

  EXPECT_EQ(DataTypeController::NOT_RUNNING, extension_dtc_->state());

  EXPECT_CALL(start_callback_, Run(DataTypeController::OK, _));
  extension_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  EXPECT_EQ(DataTypeController::RUNNING, extension_dtc_->state());
  extension_dtc_->Stop();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, extension_dtc_->state());
}

// TODO(akalin): Add this test to all the other DTCs.
TEST_F(ExtensionDataTypeControllerTest, OnUnrecoverableError) {
  SetStartExpectations();
  SetAssociateExpectations();
  SetActivateExpectations();
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillRepeatedly(DoAll(SetArgumentPointee<0>(true), Return(true)));
  EXPECT_CALL(service_, OnUnrecoverableError(_, _)).
      WillOnce(InvokeWithoutArgs(extension_dtc_.get(),
                                 &ExtensionDataTypeController::Stop));
  SetStopExpectations();

  EXPECT_CALL(start_callback_, Run(DataTypeController::OK, _));
  extension_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  // This should cause extension_dtc_->Stop() to be called.
  extension_dtc_->OnUnrecoverableError(FROM_HERE, "Test");
  PumpLoop();
}
