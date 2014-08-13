// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/tracked_objects.h"
#include "chrome/browser/sync/glue/frontend_data_type_controller.h"
#include "chrome/browser/sync/glue/frontend_data_type_controller_mock.h"
#include "chrome/browser/sync/profile_sync_components_factory_mock.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/test/base/profile_mock.h"
#include "components/sync_driver/change_processor_mock.h"
#include "components/sync_driver/data_type_controller_mock.h"
#include "components/sync_driver/model_associator_mock.h"
#include "content/public/test/test_browser_thread_bundle.h"

using browser_sync::FrontendDataTypeController;
using browser_sync::FrontendDataTypeControllerMock;
using sync_driver::ChangeProcessorMock;
using sync_driver::DataTypeController;
using sync_driver::ModelAssociatorMock;
using sync_driver::ModelLoadCallbackMock;
using sync_driver::StartCallbackMock;
using testing::_;
using testing::DoAll;
using testing::InvokeWithoutArgs;
using testing::Return;
using testing::SetArgumentPointee;
using testing::StrictMock;

class FrontendDataTypeControllerFake : public FrontendDataTypeController {
 public:
  FrontendDataTypeControllerFake(
      ProfileSyncComponentsFactory* profile_sync_factory,
      Profile* profile,
      ProfileSyncService* sync_service,
      FrontendDataTypeControllerMock* mock)
      : FrontendDataTypeController(base::MessageLoopProxy::current(),
                                   base::Closure(),
                                   profile_sync_factory,
                                   profile,
                                   sync_service),
        mock_(mock) {}
  virtual syncer::ModelType type() const OVERRIDE { return syncer::BOOKMARKS; }

 private:
  virtual void CreateSyncComponents() OVERRIDE {
    ProfileSyncComponentsFactory::SyncComponents sync_components =
        profile_sync_factory_->
            CreateBookmarkSyncComponents(sync_service_, this);
    model_associator_.reset(sync_components.model_associator);
    change_processor_.reset(sync_components.change_processor);
  }

  // We mock the following methods because their default implementations do
  // nothing, but we still want to make sure they're called appropriately.
  virtual bool StartModels() OVERRIDE {
    return mock_->StartModels();
  }
  virtual void CleanUpState() OVERRIDE {
    mock_->CleanUpState();
  }
  virtual void RecordUnrecoverableError(
      const tracked_objects::Location& from_here,
      const std::string& message) OVERRIDE {
    mock_->RecordUnrecoverableError(from_here, message);
  }
  virtual void RecordAssociationTime(base::TimeDelta time) OVERRIDE {
    mock_->RecordAssociationTime(time);
  }
  virtual void RecordStartFailure(
      DataTypeController::ConfigureResult result) OVERRIDE {
    mock_->RecordStartFailure(result);
  }
 private:
  virtual ~FrontendDataTypeControllerFake() {}
  FrontendDataTypeControllerMock* mock_;
};

class SyncFrontendDataTypeControllerTest : public testing::Test {
 public:
  SyncFrontendDataTypeControllerTest()
      : thread_bundle_(content::TestBrowserThreadBundle::DEFAULT),
        service_(&profile_) {}

  virtual void SetUp() {
    profile_sync_factory_.reset(new ProfileSyncComponentsFactoryMock());
    dtc_mock_ = new StrictMock<FrontendDataTypeControllerMock>();
    frontend_dtc_ =
        new FrontendDataTypeControllerFake(profile_sync_factory_.get(),
                                           &profile_,
                                           &service_,
                                           dtc_mock_.get());
  }

 protected:
  void SetStartExpectations() {
    EXPECT_CALL(*dtc_mock_.get(), StartModels()).WillOnce(Return(true));
    EXPECT_CALL(model_load_callback_, Run(_, _));
    model_associator_ = new ModelAssociatorMock();
    change_processor_ = new ChangeProcessorMock();
    EXPECT_CALL(*profile_sync_factory_, CreateBookmarkSyncComponents(_, _)).
        WillOnce(Return(ProfileSyncComponentsFactory::SyncComponents(
            model_associator_, change_processor_)));
  }

  void SetAssociateExpectations() {
    EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary()).
        WillOnce(Return(true));
    EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
        WillOnce(DoAll(SetArgumentPointee<0>(true), Return(true)));
    EXPECT_CALL(*model_associator_, AssociateModels(_, _)).
        WillOnce(Return(syncer::SyncError()));
    EXPECT_CALL(*dtc_mock_.get(), RecordAssociationTime(_));
  }

  void SetActivateExpectations(DataTypeController::ConfigureResult result) {
    EXPECT_CALL(start_callback_, Run(result, _, _));
  }

  void SetStopExpectations() {
    EXPECT_CALL(*dtc_mock_.get(), CleanUpState());
    EXPECT_CALL(service_, DeactivateDataType(_));
    EXPECT_CALL(*model_associator_, DisassociateModels()).
                WillOnce(Return(syncer::SyncError()));
  }

  void SetStartFailExpectations(DataTypeController::ConfigureResult result) {
    if (DataTypeController::IsUnrecoverableResult(result))
      EXPECT_CALL(*dtc_mock_.get(), RecordUnrecoverableError(_, _));
    EXPECT_CALL(*dtc_mock_.get(), CleanUpState());
    EXPECT_CALL(*dtc_mock_.get(), RecordStartFailure(result));
    EXPECT_CALL(start_callback_, Run(result, _, _));
  }

  void Start() {
    frontend_dtc_->LoadModels(
        base::Bind(&ModelLoadCallbackMock::Run,
                   base::Unretained(&model_load_callback_)));
    frontend_dtc_->StartAssociating(
        base::Bind(&StartCallbackMock::Run,
                   base::Unretained(&start_callback_)));
  }

  void PumpLoop() { base::MessageLoop::current()->RunUntilIdle(); }

  content::TestBrowserThreadBundle thread_bundle_;
  scoped_refptr<FrontendDataTypeControllerFake> frontend_dtc_;
  scoped_ptr<ProfileSyncComponentsFactoryMock> profile_sync_factory_;
  scoped_refptr<FrontendDataTypeControllerMock> dtc_mock_;
  ProfileMock profile_;
  ProfileSyncServiceMock service_;
  ModelAssociatorMock* model_associator_;
  ChangeProcessorMock* change_processor_;
  StartCallbackMock start_callback_;
  ModelLoadCallbackMock model_load_callback_;
};

TEST_F(SyncFrontendDataTypeControllerTest, StartOk) {
  SetStartExpectations();
  SetAssociateExpectations();
  SetActivateExpectations(DataTypeController::OK);
  EXPECT_EQ(DataTypeController::NOT_RUNNING, frontend_dtc_->state());
  Start();
  EXPECT_EQ(DataTypeController::RUNNING, frontend_dtc_->state());
}

TEST_F(SyncFrontendDataTypeControllerTest, StartFirstRun) {
  SetStartExpectations();
  EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary()).
      WillOnce(Return(true));
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(false), Return(true)));
  EXPECT_CALL(*model_associator_, AssociateModels(_, _)).
      WillOnce(Return(syncer::SyncError()));
  EXPECT_CALL(*dtc_mock_.get(), RecordAssociationTime(_));
  SetActivateExpectations(DataTypeController::OK_FIRST_RUN);
  EXPECT_EQ(DataTypeController::NOT_RUNNING, frontend_dtc_->state());
  Start();
  EXPECT_EQ(DataTypeController::RUNNING, frontend_dtc_->state());
}

TEST_F(SyncFrontendDataTypeControllerTest, AbortDuringStartModels) {
  EXPECT_CALL(*dtc_mock_.get(), StartModels()).WillOnce(Return(false));
  EXPECT_CALL(*dtc_mock_.get(), CleanUpState());
  EXPECT_CALL(model_load_callback_, Run(_, _));
  EXPECT_EQ(DataTypeController::NOT_RUNNING, frontend_dtc_->state());
  frontend_dtc_->LoadModels(
      base::Bind(&ModelLoadCallbackMock::Run,
                 base::Unretained(&model_load_callback_)));
  EXPECT_EQ(DataTypeController::MODEL_STARTING, frontend_dtc_->state());
  frontend_dtc_->Stop();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, frontend_dtc_->state());
}

TEST_F(SyncFrontendDataTypeControllerTest, StartAssociationFailed) {
  SetStartExpectations();
  EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary()).
      WillOnce(Return(true));
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(true), Return(true)));
  EXPECT_CALL(*model_associator_, AssociateModels(_, _)).
      WillOnce(Return(syncer::SyncError(FROM_HERE,
                                        syncer::SyncError::DATATYPE_ERROR,
                                        "error",
                                        syncer::BOOKMARKS)));

  EXPECT_CALL(*dtc_mock_.get(), RecordAssociationTime(_));
  SetStartFailExpectations(DataTypeController::ASSOCIATION_FAILED);
  // Set up association to fail with an association failed error.
  EXPECT_EQ(DataTypeController::NOT_RUNNING, frontend_dtc_->state());
  Start();
  EXPECT_EQ(DataTypeController::DISABLED, frontend_dtc_->state());
}

TEST_F(SyncFrontendDataTypeControllerTest,
       StartAssociationTriggersUnrecoverableError) {
  SetStartExpectations();
  SetStartFailExpectations(DataTypeController::UNRECOVERABLE_ERROR);
  // Set up association to fail with an unrecoverable error.
  EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary()).
      WillRepeatedly(Return(true));
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillRepeatedly(DoAll(SetArgumentPointee<0>(false), Return(false)));
  EXPECT_EQ(DataTypeController::NOT_RUNNING, frontend_dtc_->state());
  Start();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, frontend_dtc_->state());
}

TEST_F(SyncFrontendDataTypeControllerTest, StartAssociationCryptoNotReady) {
  SetStartExpectations();
  SetStartFailExpectations(DataTypeController::NEEDS_CRYPTO);
  // Set up association to fail with a NEEDS_CRYPTO error.
  EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary()).
      WillRepeatedly(Return(false));
  EXPECT_EQ(DataTypeController::NOT_RUNNING, frontend_dtc_->state());
  Start();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, frontend_dtc_->state());
}

TEST_F(SyncFrontendDataTypeControllerTest, Stop) {
  SetStartExpectations();
  SetAssociateExpectations();
  SetActivateExpectations(DataTypeController::OK);
  SetStopExpectations();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, frontend_dtc_->state());
  Start();
  EXPECT_EQ(DataTypeController::RUNNING, frontend_dtc_->state());
  frontend_dtc_->Stop();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, frontend_dtc_->state());
}
