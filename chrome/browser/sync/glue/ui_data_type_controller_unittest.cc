// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/ui_data_type_controller.h"

#include "base/bind.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/tracked_objects.h"
#include "chrome/browser/sync/glue/data_type_controller_mock.h"
#include "chrome/browser/sync/glue/fake_generic_change_processor.h"
#include "chrome/browser/sync/profile_sync_components_factory_mock.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/test/base/profile_mock.h"
#include "content/test/test_browser_thread.h"
#include "sync/api/fake_syncable_service.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using testing::_;
using testing::InvokeWithoutArgs;
using testing::Return;

namespace browser_sync {
namespace {

ACTION(MakeSharedChangeProcessor) {
  return new SharedChangeProcessor();
}

ACTION_P(ReturnAndRelease, change_processor) {
  return change_processor->release();
}

// TODO(zea): Expand this to make the dtc type paramterizable. This will let us
// test the basic functionality of all UIDataTypeControllers. We'll need to have
// intelligent default values for the methods queried in the dependent services
// (e.g. those queried in StartModels).
class SyncUIDataTypeControllerTest : public testing::Test {
 public:
  SyncUIDataTypeControllerTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        type_(syncable::PREFERENCES),
        change_processor_(new FakeGenericChangeProcessor()) {}

  virtual void SetUp() {
    profile_sync_factory_.reset(new ProfileSyncComponentsFactoryMock());
    preference_dtc_ =
        new UIDataTypeController(type_,
                                 profile_sync_factory_.get(),
                                 &profile_,
                                 &profile_sync_service_);
    SetStartExpectations();
  }

  virtual void TearDown() {
    // Must be done before we pump the loop.
    syncable_service_.StopSyncing(type_);
    preference_dtc_ = NULL;
    PumpLoop();
  }

 protected:
  void SetStartExpectations() {
    // Ownership gets passed to caller of CreateGenericChangeProcessor.
    change_processor_.reset(new FakeGenericChangeProcessor());
    EXPECT_CALL(*profile_sync_factory_, GetSyncableServiceForType(type_)).
        WillOnce(Return(syncable_service_.AsWeakPtr()));
    EXPECT_CALL(*profile_sync_factory_, CreateSharedChangeProcessor()).
        WillOnce(MakeSharedChangeProcessor());
    EXPECT_CALL(*profile_sync_factory_, CreateGenericChangeProcessor(_, _, _)).
        WillOnce(ReturnAndRelease(&change_processor_));
  }

  void SetActivateExpectations() {
    EXPECT_CALL(profile_sync_service_, ActivateDataType(type_, _, _));
  }

  void SetStopExpectations() {
    EXPECT_CALL(profile_sync_service_, DeactivateDataType(type_));
  }

  void PumpLoop() {
    message_loop_.RunAllPending();
  }

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  ProfileMock profile_;
  scoped_ptr<ProfileSyncComponentsFactoryMock> profile_sync_factory_;
  ProfileSyncServiceMock profile_sync_service_;
  const syncable::ModelType type_;
  StartCallbackMock start_callback_;
  scoped_refptr<UIDataTypeController> preference_dtc_;
  scoped_ptr<FakeGenericChangeProcessor> change_processor_;
  FakeSyncableService syncable_service_;
};

// Start the DTC. Verify that the callback is called with OK, the
// service has been told to start syncing and that the DTC is now in RUNNING
// state.
TEST_F(SyncUIDataTypeControllerTest, Start) {
  SetActivateExpectations();
  EXPECT_CALL(start_callback_, Run(DataTypeController::OK, _));

  EXPECT_EQ(DataTypeController::NOT_RUNNING, preference_dtc_->state());
  EXPECT_FALSE(syncable_service_.syncing());
  preference_dtc_->Start(
      base::Bind(&StartCallbackMock::Run, base::Unretained(&start_callback_)));
  EXPECT_EQ(DataTypeController::RUNNING, preference_dtc_->state());
  EXPECT_TRUE(syncable_service_.syncing());
}

// Start and then stop the DTC. Verify that the service started and stopped
// syncing, and that the DTC went from RUNNING to NOT_RUNNING.
TEST_F(SyncUIDataTypeControllerTest, StartStop) {
  SetActivateExpectations();
  SetStopExpectations();
  EXPECT_CALL(start_callback_, Run(DataTypeController::OK, _));

  EXPECT_EQ(DataTypeController::NOT_RUNNING, preference_dtc_->state());
  EXPECT_FALSE(syncable_service_.syncing());
  preference_dtc_->Start(
      base::Bind(&StartCallbackMock::Run, base::Unretained(&start_callback_)));
  EXPECT_EQ(DataTypeController::RUNNING, preference_dtc_->state());
  EXPECT_TRUE(syncable_service_.syncing());
  preference_dtc_->Stop();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, preference_dtc_->state());
  EXPECT_FALSE(syncable_service_.syncing());
}

// Start the DTC when no user nodes are created. Verify that the callback
// is called with OK_FIRST_RUN. Stop the DTC.
TEST_F(SyncUIDataTypeControllerTest, StartStopFirstRun) {
  SetActivateExpectations();
  SetStopExpectations();
  EXPECT_CALL(start_callback_, Run(DataTypeController::OK_FIRST_RUN, _));
  change_processor_->set_sync_model_has_user_created_nodes(false);

  EXPECT_EQ(DataTypeController::NOT_RUNNING, preference_dtc_->state());
  EXPECT_FALSE(syncable_service_.syncing());
  preference_dtc_->Start(
      base::Bind(&StartCallbackMock::Run, base::Unretained(&start_callback_)));
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
  SetStopExpectations();
  EXPECT_CALL(start_callback_,
              Run(DataTypeController::ASSOCIATION_FAILED, _));
  syncable_service_.set_merge_data_and_start_syncing_error(
      SyncError(FROM_HERE, "Error", type_));

  EXPECT_EQ(DataTypeController::NOT_RUNNING, preference_dtc_->state());
  EXPECT_FALSE(syncable_service_.syncing());
  preference_dtc_->Start(
      base::Bind(&StartCallbackMock::Run, base::Unretained(&start_callback_)));
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
              Run(DataTypeController::UNRECOVERABLE_ERROR, _));
  change_processor_->set_sync_model_has_user_created_nodes_success(false);

  EXPECT_EQ(DataTypeController::NOT_RUNNING, preference_dtc_->state());
  EXPECT_FALSE(syncable_service_.syncing());
  preference_dtc_->Start(
      base::Bind(&StartCallbackMock::Run, base::Unretained(&start_callback_)));
  EXPECT_EQ(DataTypeController::NOT_RUNNING, preference_dtc_->state());
  EXPECT_FALSE(syncable_service_.syncing());
}

// Start the DTC, but then trigger an unrecoverable error. Verify the syncer
// gets stopped and the DTC is in NOT_RUNNING state.
TEST_F(SyncUIDataTypeControllerTest, OnUnrecoverableError) {
  SetActivateExpectations();
  EXPECT_CALL(profile_sync_service_, OnUnrecoverableError(_,_)).
      WillOnce(InvokeWithoutArgs(preference_dtc_.get(),
                                 &UIDataTypeController::Stop));
  SetStopExpectations();
  EXPECT_CALL(start_callback_, Run(DataTypeController::OK, _));

  EXPECT_EQ(DataTypeController::NOT_RUNNING, preference_dtc_->state());
  EXPECT_FALSE(syncable_service_.syncing());
  preference_dtc_->Start(
      base::Bind(&StartCallbackMock::Run, base::Unretained(&start_callback_)));
  EXPECT_TRUE(syncable_service_.syncing());
  preference_dtc_->OnUnrecoverableError(FROM_HERE, "Test");
  PumpLoop();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, preference_dtc_->state());
  EXPECT_FALSE(syncable_service_.syncing());
}

}  // namespace
}  // namespace browser_sync
