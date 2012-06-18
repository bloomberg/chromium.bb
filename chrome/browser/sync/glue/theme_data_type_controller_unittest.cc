// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "chrome/browser/sync/glue/change_processor_mock.h"
#include "chrome/browser/sync/glue/data_type_controller_mock.h"
#include "chrome/browser/sync/glue/model_associator_mock.h"
#include "chrome/browser/sync/glue/theme_data_type_controller.h"
#include "chrome/browser/sync/profile_sync_components_factory_mock.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/test/base/profile_mock.h"
#include "content/public/test/test_browser_thread.h"

using browser_sync::ThemeDataTypeController;
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

class SyncThemeDataTypeControllerTest : public testing::Test {
 public:
  SyncThemeDataTypeControllerTest()
      : ui_thread_(BrowserThread::UI, &message_loop_) {}

  virtual void SetUp() {
    profile_sync_factory_.reset(new ProfileSyncComponentsFactoryMock());
    theme_dtc_ =
        new ThemeDataTypeController(profile_sync_factory_.get(),
                                    &profile_, &service_);
  }

 protected:
  void SetStartExpectations() {
    model_associator_ = new ModelAssociatorMock();
    change_processor_ = new ChangeProcessorMock();
    EXPECT_CALL(model_load_callback_, Run(_, _));
    EXPECT_CALL(*profile_sync_factory_, CreateThemeSyncComponents(_, _)).
        WillOnce(Return(ProfileSyncComponentsFactory::SyncComponents(
            model_associator_, change_processor_)));
  }

  void SetAssociateExpectations() {
    EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary()).
        WillRepeatedly(Return(true));
    EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
        WillRepeatedly(DoAll(SetArgumentPointee<0>(true), Return(true)));
    EXPECT_CALL(*model_associator_, AssociateModels()).
        WillRepeatedly(Return(SyncError()));
  }

  void SetActivateExpectations() {
    EXPECT_CALL(service_, ActivateDataType(_, _, _));
  }

  void SetStopExpectations() {
    EXPECT_CALL(service_, DeactivateDataType(_));
    EXPECT_CALL(*model_associator_, DisassociateModels()).
                WillOnce(Return(SyncError()));
  }

  void PumpLoop() {
    message_loop_.RunAllPending();
  }

  void Start() {
    theme_dtc_->LoadModels(
        base::Bind(&ModelLoadCallbackMock::Run,
                   base::Unretained(&model_load_callback_)));
    theme_dtc_->StartAssociating(
        base::Bind(&StartCallbackMock::Run,
                   base::Unretained(&start_callback_)));
  }

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  scoped_refptr<ThemeDataTypeController> theme_dtc_;
  scoped_ptr<ProfileSyncComponentsFactoryMock> profile_sync_factory_;
  ProfileMock profile_;
  ProfileSyncServiceMock service_;
  ModelAssociatorMock* model_associator_;
  ChangeProcessorMock* change_processor_;
  StartCallbackMock start_callback_;
  ModelLoadCallbackMock model_load_callback_;
};

TEST_F(SyncThemeDataTypeControllerTest, Start) {
  SetStartExpectations();
  SetAssociateExpectations();
  SetActivateExpectations();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, theme_dtc_->state());
  EXPECT_CALL(start_callback_, Run(DataTypeController::OK, _));
  Start();
  EXPECT_EQ(DataTypeController::RUNNING, theme_dtc_->state());
}

TEST_F(SyncThemeDataTypeControllerTest, StartFirstRun) {
  SetStartExpectations();
  SetAssociateExpectations();
  SetActivateExpectations();
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillRepeatedly(DoAll(SetArgumentPointee<0>(false), Return(true)));
  EXPECT_CALL(start_callback_, Run(DataTypeController::OK_FIRST_RUN, _));
  Start();
}

TEST_F(SyncThemeDataTypeControllerTest, StartOk) {
  SetStartExpectations();
  SetAssociateExpectations();
  SetActivateExpectations();
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillRepeatedly(DoAll(SetArgumentPointee<0>(true), Return(true)));

  EXPECT_CALL(start_callback_, Run(DataTypeController::OK, _));
  Start();
}

TEST_F(SyncThemeDataTypeControllerTest, StartAssociationFailed) {
  SetStartExpectations();
  SetAssociateExpectations();
  EXPECT_CALL(*model_associator_, AssociateModels()).
      WillRepeatedly(Return(SyncError(FROM_HERE, "Error", syncable::THEMES)));

  EXPECT_CALL(start_callback_,
              Run(DataTypeController::ASSOCIATION_FAILED, _));
  Start();
  EXPECT_EQ(DataTypeController::DISABLED, theme_dtc_->state());
}

TEST_F(SyncThemeDataTypeControllerTest,
       StartAssociationTriggersUnrecoverableError) {
  SetStartExpectations();
  // Set up association to fail with an unrecoverable error.
  EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary()).
      WillRepeatedly(Return(true));
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillRepeatedly(DoAll(SetArgumentPointee<0>(false), Return(false)));
  EXPECT_CALL(start_callback_,
              Run(DataTypeController::UNRECOVERABLE_ERROR, _));
  Start();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, theme_dtc_->state());
}

TEST_F(SyncThemeDataTypeControllerTest, Stop) {
  SetStartExpectations();
  SetAssociateExpectations();
  SetActivateExpectations();
  SetStopExpectations();

  EXPECT_EQ(DataTypeController::NOT_RUNNING, theme_dtc_->state());

  EXPECT_CALL(start_callback_, Run(DataTypeController::OK, _));
  Start();
  EXPECT_EQ(DataTypeController::RUNNING, theme_dtc_->state());
  theme_dtc_->Stop();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, theme_dtc_->state());
}

// TODO(akalin): Add this test to all the other DTCs.
TEST_F(SyncThemeDataTypeControllerTest, OnSingleDatatypeUnrecoverableError) {
  SetStartExpectations();
  SetAssociateExpectations();
  SetActivateExpectations();
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillRepeatedly(DoAll(SetArgumentPointee<0>(true), Return(true)));
  EXPECT_CALL(service_, DisableBrokenDatatype(_,_,_)).
      WillOnce(InvokeWithoutArgs(theme_dtc_.get(),
      &ThemeDataTypeController::Stop));
  SetStopExpectations();

  EXPECT_CALL(start_callback_, Run(DataTypeController::OK, _));
  Start();
  // This should cause theme_dtc_->Stop() to be called.
  theme_dtc_->OnSingleDatatypeUnrecoverableError(FROM_HERE, "Test");
  PumpLoop();
}
