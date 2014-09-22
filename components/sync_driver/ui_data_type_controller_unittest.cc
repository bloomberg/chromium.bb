// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/ui_data_type_controller.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/tracked_objects.h"
#include "components/sync_driver/data_type_controller_mock.h"
#include "components/sync_driver/fake_generic_change_processor.h"
#include "sync/api/fake_syncable_service.h"
#include "sync/internal_api/public/attachments/attachment_service_impl.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::_;
using testing::Invoke;
using testing::InvokeWithoutArgs;
using testing::Return;

namespace sync_driver {
namespace {

// TODO(zea): Expand this to make the dtc type paramterizable. This will let us
// test the basic functionality of all UIDataTypeControllers. We'll need to have
// intelligent default values for the methods queried in the dependent services
// (e.g. those queried in StartModels).
class SyncUIDataTypeControllerTest : public testing::Test,
                                     public SyncApiComponentFactory {
 public:
  SyncUIDataTypeControllerTest()
      : type_(syncer::PREFERENCES),
        change_processor_(NULL) {}

  virtual void SetUp() {
    preference_dtc_ =
        new UIDataTypeController(
            base::MessageLoopProxy::current(),
            base::Closure(),
            type_,
            this);
    SetStartExpectations();
  }

  virtual void TearDown() {
    // Must be done before we pump the loop.
    syncable_service_.StopSyncing(type_);
    preference_dtc_ = NULL;
    PumpLoop();
  }

  virtual base::WeakPtr<syncer::SyncableService> GetSyncableServiceForType(
      syncer::ModelType type) OVERRIDE {
    return syncable_service_.AsWeakPtr();
  }

  virtual scoped_ptr<syncer::AttachmentService> CreateAttachmentService(
      const scoped_refptr<syncer::AttachmentStore>& attachment_store,
      const syncer::UserShare& user_share,
      syncer::AttachmentService::Delegate* delegate) OVERRIDE {
    return syncer::AttachmentServiceImpl::CreateForTest();
  }

 protected:
  void SetStartExpectations() {
    scoped_ptr<FakeGenericChangeProcessor> p(
        new FakeGenericChangeProcessor(type_, this));
    change_processor_ = p.get();
    scoped_ptr<GenericChangeProcessorFactory> f(
        new FakeGenericChangeProcessorFactory(p.Pass()));
    preference_dtc_->SetGenericChangeProcessorFactoryForTest(f.Pass());
    EXPECT_CALL(model_load_callback_, Run(_, _));
  }

  void Start() {
    preference_dtc_->LoadModels(
        base::Bind(&ModelLoadCallbackMock::Run,
                   base::Unretained(&model_load_callback_)));
    preference_dtc_->StartAssociating(
        base::Bind(&StartCallbackMock::Run,
                   base::Unretained(&start_callback_)));
  }

  void PumpLoop() {
    message_loop_.RunUntilIdle();
  }

  base::MessageLoopForUI message_loop_;
  const syncer::ModelType type_;
  StartCallbackMock start_callback_;
  ModelLoadCallbackMock model_load_callback_;
  scoped_refptr<UIDataTypeController> preference_dtc_;
  FakeGenericChangeProcessor* change_processor_;
  syncer::FakeSyncableService syncable_service_;
};

// Start the DTC. Verify that the callback is called with OK, the
// service has been told to start syncing and that the DTC is now in RUNNING
// state.
TEST_F(SyncUIDataTypeControllerTest, Start) {
  EXPECT_CALL(start_callback_, Run(DataTypeController::OK, _, _));

  EXPECT_EQ(DataTypeController::NOT_RUNNING, preference_dtc_->state());
  EXPECT_FALSE(syncable_service_.syncing());
  Start();
  EXPECT_EQ(DataTypeController::RUNNING, preference_dtc_->state());
  EXPECT_TRUE(syncable_service_.syncing());
}

// Start and then stop the DTC. Verify that the service started and stopped
// syncing, and that the DTC went from RUNNING to NOT_RUNNING.
TEST_F(SyncUIDataTypeControllerTest, StartStop) {
  EXPECT_CALL(start_callback_, Run(DataTypeController::OK, _, _));

  EXPECT_EQ(DataTypeController::NOT_RUNNING, preference_dtc_->state());
  EXPECT_FALSE(syncable_service_.syncing());
  Start();
  EXPECT_EQ(DataTypeController::RUNNING, preference_dtc_->state());
  EXPECT_TRUE(syncable_service_.syncing());
  preference_dtc_->Stop();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, preference_dtc_->state());
  EXPECT_FALSE(syncable_service_.syncing());
}

// Start the DTC when no user nodes are created. Verify that the callback
// is called with OK_FIRST_RUN. Stop the DTC.
TEST_F(SyncUIDataTypeControllerTest, StartStopFirstRun) {
  EXPECT_CALL(start_callback_, Run(DataTypeController::OK_FIRST_RUN, _, _));
  change_processor_->set_sync_model_has_user_created_nodes(false);

  EXPECT_EQ(DataTypeController::NOT_RUNNING, preference_dtc_->state());
  EXPECT_FALSE(syncable_service_.syncing());
  Start();
  EXPECT_EQ(DataTypeController::RUNNING, preference_dtc_->state());
  EXPECT_TRUE(syncable_service_.syncing());
  preference_dtc_->Stop();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, preference_dtc_->state());
  EXPECT_FALSE(syncable_service_.syncing());
}

// Start the DTC, but have the service fail association. Verify the callback
// is called with ASSOCIATION_FAILED, the DTC goes to state DISABLED, and the
// service is not syncing. Then stop the DTC.
TEST_F(SyncUIDataTypeControllerTest, StartAssociationFailed) {
  EXPECT_CALL(start_callback_,
              Run(DataTypeController::ASSOCIATION_FAILED, _, _));
  syncable_service_.set_merge_data_and_start_syncing_error(
      syncer::SyncError(FROM_HERE,
                        syncer::SyncError::DATATYPE_ERROR,
                        "Error",
                        type_));

  EXPECT_EQ(DataTypeController::NOT_RUNNING, preference_dtc_->state());
  EXPECT_FALSE(syncable_service_.syncing());
  Start();
  EXPECT_EQ(DataTypeController::DISABLED, preference_dtc_->state());
  EXPECT_FALSE(syncable_service_.syncing());
  preference_dtc_->Stop();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, preference_dtc_->state());
  EXPECT_FALSE(syncable_service_.syncing());
}

// Start the DTC but fail to check if there are user created nodes. Verify the
// DTC calls the callback with UNRECOVERABLE_ERROR and that it goes into
// NOT_RUNNING state. Verify the syncable service is not syncing.
TEST_F(SyncUIDataTypeControllerTest,
       StartAssociationTriggersUnrecoverableError) {
  EXPECT_CALL(start_callback_,
              Run(DataTypeController::UNRECOVERABLE_ERROR, _, _));
  change_processor_->set_sync_model_has_user_created_nodes_success(false);

  EXPECT_EQ(DataTypeController::NOT_RUNNING, preference_dtc_->state());
  EXPECT_FALSE(syncable_service_.syncing());
  Start();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, preference_dtc_->state());
  EXPECT_FALSE(syncable_service_.syncing());
}

// Start the DTC, but then trigger an unrecoverable error. Verify the syncer
// gets stopped and the DTC is in NOT_RUNNING state.
TEST_F(SyncUIDataTypeControllerTest, OnSingleDatatypeUnrecoverableError) {
  EXPECT_CALL(start_callback_, Run(DataTypeController::OK, _, _));

  EXPECT_EQ(DataTypeController::NOT_RUNNING, preference_dtc_->state());
  EXPECT_FALSE(syncable_service_.syncing());
  Start();
  EXPECT_TRUE(syncable_service_.syncing());

  testing::Mock::VerifyAndClearExpectations(&start_callback_);
  EXPECT_CALL(model_load_callback_, Run(_, _));
  syncer::SyncError error(FROM_HERE,
                          syncer::SyncError::DATATYPE_ERROR,
                          "error",
                          syncer::PREFERENCES);
  preference_dtc_->OnSingleDataTypeUnrecoverableError(error);
}

}  // namespace
}  // namespace sync_driver
