// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync/driver/frontend_data_type_controller.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "components/sync/driver/data_type_controller_mock.h"
#include "components/sync/driver/fake_sync_client.h"
#include "components/sync/driver/fake_sync_service.h"
#include "components/sync/driver/frontend_data_type_controller_mock.h"
#include "components/sync/driver/model_associator_mock.h"
#include "components/sync/driver/sync_api_component_factory_mock.h"
#include "components/sync/model/change_processor_mock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::DoAll;
using testing::InvokeWithoutArgs;
using testing::Return;
using testing::SetArgPointee;
using testing::StrictMock;

namespace syncer {

class FrontendDataTypeControllerFake : public FrontendDataTypeController {
 public:
  FrontendDataTypeControllerFake(SyncClient* sync_client,
                                 FrontendDataTypeControllerMock* mock)
      : FrontendDataTypeController(BOOKMARKS, base::Closure(), sync_client),
        mock_(mock),
        sync_client_(sync_client) {}
  ~FrontendDataTypeControllerFake() override {}

 private:
  void CreateSyncComponents() override {
    SyncApiComponentFactory::SyncComponents sync_components =
        sync_client_->GetSyncApiComponentFactory()
            ->CreateBookmarkSyncComponents(nullptr, CreateErrorHandler());
    model_associator_.reset(sync_components.model_associator);
    change_processor_.reset(sync_components.change_processor);
  }

  // We mock the following methods because their default implementations do
  // nothing, but we still want to make sure they're called appropriately.
  bool StartModels() override { return mock_->StartModels(); }
  void CleanUpState() override { mock_->CleanUpState(); }
  void RecordAssociationTime(base::TimeDelta time) override {
    mock_->RecordAssociationTime(time);
  }
  void RecordStartFailure(DataTypeController::ConfigureResult result) override {
    mock_->RecordStartFailure(result);
  }

  FrontendDataTypeControllerMock* mock_;
  SyncClient* sync_client_;
};

class SyncFrontendDataTypeControllerTest : public testing::Test {
 public:
  SyncFrontendDataTypeControllerTest()
      : model_associator_(new ModelAssociatorMock()),
        change_processor_(new ChangeProcessorMock()),
        components_factory_(model_associator_, change_processor_),
        sync_client_(&components_factory_) {}

  void SetUp() override {
    dtc_mock_ = std::make_unique<StrictMock<FrontendDataTypeControllerMock>>();
    frontend_dtc_ = std::make_unique<FrontendDataTypeControllerFake>(
        &sync_client_, dtc_mock_.get());
  }

 protected:
  void SetStartExpectations() {
    EXPECT_CALL(*dtc_mock_.get(), StartModels()).WillOnce(Return(true));
    EXPECT_CALL(model_load_callback_, Run(_, _));
  }

  void SetAssociateExpectations() {
    EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary())
        .WillOnce(Return(true));
    EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_))
        .WillOnce(DoAll(SetArgPointee<0>(true), Return(true)));
    EXPECT_CALL(*model_associator_, AssociateModels(_, _))
        .WillOnce(Return(SyncError()));
    EXPECT_CALL(*dtc_mock_.get(), RecordAssociationTime(_));
  }

  void SetActivateExpectations(DataTypeController::ConfigureResult result) {
    EXPECT_CALL(start_callback_, Run(result, _, _));
  }

  void SetStopExpectations() {
    EXPECT_CALL(*dtc_mock_.get(), CleanUpState());
    EXPECT_CALL(*model_associator_, DisassociateModels())
        .WillOnce(Return(SyncError()));
  }

  void SetStartFailExpectations(DataTypeController::ConfigureResult result) {
    EXPECT_CALL(*dtc_mock_.get(), CleanUpState());
    EXPECT_CALL(*dtc_mock_.get(), RecordStartFailure(result));
    EXPECT_CALL(start_callback_, Run(result, _, _));
  }

  void Start() {
    frontend_dtc_->LoadModels(base::Bind(
        &ModelLoadCallbackMock::Run, base::Unretained(&model_load_callback_)));
    frontend_dtc_->StartAssociating(base::Bind(
        &StartCallbackMock::Run, base::Unretained(&start_callback_)));
    PumpLoop();
  }

  void PumpLoop() { base::RunLoop().RunUntilIdle(); }

  base::MessageLoop message_loop_;
  ModelAssociatorMock* model_associator_;
  ChangeProcessorMock* change_processor_;
  SyncApiComponentFactoryMock components_factory_;
  FakeSyncClient sync_client_;
  std::unique_ptr<FrontendDataTypeControllerFake> frontend_dtc_;
  std::unique_ptr<FrontendDataTypeControllerMock> dtc_mock_;
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
  EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary())
      .WillOnce(Return(true));
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_))
      .WillOnce(DoAll(SetArgPointee<0>(false), Return(true)));
  EXPECT_CALL(*model_associator_, AssociateModels(_, _))
      .WillOnce(Return(SyncError()));
  EXPECT_CALL(*dtc_mock_.get(), RecordAssociationTime(_));
  SetActivateExpectations(DataTypeController::OK_FIRST_RUN);
  EXPECT_EQ(DataTypeController::NOT_RUNNING, frontend_dtc_->state());
  Start();
  EXPECT_EQ(DataTypeController::RUNNING, frontend_dtc_->state());
}

TEST_F(SyncFrontendDataTypeControllerTest, StartStopBeforeAssociation) {
  EXPECT_CALL(*dtc_mock_.get(), StartModels()).WillOnce(Return(true));
  EXPECT_CALL(*dtc_mock_.get(), CleanUpState());
  EXPECT_CALL(model_load_callback_, Run(_, _));
  EXPECT_EQ(DataTypeController::NOT_RUNNING, frontend_dtc_->state());
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&FrontendDataTypeController::Stop,
                            base::AsWeakPtr(frontend_dtc_.get())));
  Start();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, frontend_dtc_->state());
}

TEST_F(SyncFrontendDataTypeControllerTest, AbortDuringStartModels) {
  EXPECT_CALL(*dtc_mock_.get(), StartModels()).WillOnce(Return(false));
  EXPECT_CALL(*dtc_mock_.get(), CleanUpState());
  EXPECT_EQ(DataTypeController::NOT_RUNNING, frontend_dtc_->state());
  frontend_dtc_->LoadModels(base::Bind(
      &ModelLoadCallbackMock::Run, base::Unretained(&model_load_callback_)));
  EXPECT_EQ(DataTypeController::MODEL_STARTING, frontend_dtc_->state());
  frontend_dtc_->Stop();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, frontend_dtc_->state());
}

TEST_F(SyncFrontendDataTypeControllerTest, StartAssociationFailed) {
  SetStartExpectations();
  EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary())
      .WillOnce(Return(true));
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_))
      .WillOnce(DoAll(SetArgPointee<0>(true), Return(true)));
  EXPECT_CALL(*model_associator_, AssociateModels(_, _))
      .WillOnce(Return(
          SyncError(FROM_HERE, SyncError::DATATYPE_ERROR, "error", BOOKMARKS)));

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
  EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary())
      .WillRepeatedly(Return(true));
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_))
      .WillRepeatedly(DoAll(SetArgPointee<0>(false), Return(false)));
  EXPECT_EQ(DataTypeController::NOT_RUNNING, frontend_dtc_->state());
  Start();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, frontend_dtc_->state());
}

TEST_F(SyncFrontendDataTypeControllerTest, StartAssociationCryptoNotReady) {
  SetStartExpectations();
  SetStartFailExpectations(DataTypeController::NEEDS_CRYPTO);
  // Set up association to fail with a NEEDS_CRYPTO error.
  EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary())
      .WillRepeatedly(Return(false));
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

}  // namespace syncer
