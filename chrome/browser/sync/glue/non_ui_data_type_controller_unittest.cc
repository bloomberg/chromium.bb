// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/non_ui_data_type_controller.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "base/tracked_objects.h"
#include "chrome/browser/sync/glue/data_type_controller_mock.h"
#include "chrome/browser/sync/glue/non_ui_data_type_controller_mock.h"
#include "chrome/browser/sync/glue/shared_change_processor_mock.h"
#include "chrome/browser/sync/profile_sync_components_factory_mock.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/test/base/profile_mock.h"
#include "content/public/test/test_browser_thread.h"
#include "sync/api/fake_syncable_service.h"
#include "sync/internal_api/public/engine/model_safe_worker.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

namespace {

using base::WaitableEvent;
using content::BrowserThread;
using syncer::AUTOFILL_PROFILE;
using testing::_;
using testing::AtLeast;
using testing::DoAll;
using testing::InvokeWithoutArgs;
using testing::Mock;
using testing::Return;
using testing::SetArgumentPointee;
using testing::StrictMock;

ACTION_P(WaitOnEvent, event) {
  event->Wait();
}

ACTION_P(SignalEvent, event) {
  event->Signal();
}

ACTION_P(SaveChangeProcessor, scoped_change_processor) {
  scoped_change_processor->reset(arg2);
}

ACTION_P(GetWeakPtrToSyncableService, syncable_service) {
  // Have to do this within an Action to ensure it's not evaluated on the wrong
  // thread.
  return syncable_service->AsWeakPtr();
}

class NonUIDataTypeControllerFake
    : public NonUIDataTypeController {
 public:
  NonUIDataTypeControllerFake(
      ProfileSyncComponentsFactory* profile_sync_factory,
      Profile* profile,
      ProfileSyncService* sync_service,
      NonUIDataTypeControllerMock* mock)
      : NonUIDataTypeController(profile_sync_factory,
                                         profile,
                                         sync_service),
        blocked_(false),
        mock_(mock) {}

  virtual syncer::ModelType type() const OVERRIDE {
    return AUTOFILL_PROFILE;
  }
  virtual syncer::ModelSafeGroup model_safe_group() const OVERRIDE {
    return syncer::GROUP_DB;
  }

  // Prevent tasks from being posted on the backend thread until
  // UnblockBackendTasks() is called.
  void BlockBackendTasks() {
    blocked_ = true;
  }

  // Post pending tasks on the backend thread and start allowing tasks
  // to be posted on the backend thread again.
  void UnblockBackendTasks() {
    blocked_ = false;
    for (std::vector<PendingTask>::const_iterator it = pending_tasks_.begin();
         it != pending_tasks_.end(); ++it) {
      PostTaskOnBackendThread(it->from_here, it->task);
    }
    pending_tasks_.clear();
  }

 protected:
  virtual bool PostTaskOnBackendThread(
      const tracked_objects::Location& from_here,
      const base::Closure& task) OVERRIDE {
    if (blocked_) {
      pending_tasks_.push_back(PendingTask(from_here, task));
      return true;
    } else {
      return BrowserThread::PostTask(BrowserThread::DB, from_here, task);
    }
  }

  // We mock the following methods because their default implementations do
  // nothing, but we still want to make sure they're called appropriately.
  virtual bool StartModels() OVERRIDE {
    return mock_->StartModels();
  }
  virtual void StopModels() OVERRIDE {
    mock_->StopModels();
  }
  virtual void RecordUnrecoverableError(
      const tracked_objects::Location& from_here,
      const std::string& message) OVERRIDE {
    mock_->RecordUnrecoverableError(from_here, message);
  }
  virtual void RecordAssociationTime(base::TimeDelta time) OVERRIDE {
    mock_->RecordAssociationTime(time);
  }
  virtual void RecordStartFailure(DataTypeController::StartResult result)
      OVERRIDE {
    mock_->RecordStartFailure(result);
  }

 private:
  virtual ~NonUIDataTypeControllerFake() {}

  DISALLOW_COPY_AND_ASSIGN(NonUIDataTypeControllerFake);

  struct PendingTask {
    PendingTask(const tracked_objects::Location& from_here,
                const base::Closure& task)
        : from_here(from_here), task(task) {}

    tracked_objects::Location from_here;
    base::Closure task;
  };

  bool blocked_;
  std::vector<PendingTask> pending_tasks_;
  NonUIDataTypeControllerMock* mock_;
};

class SyncNonUIDataTypeControllerTest : public testing::Test {
 public:
  SyncNonUIDataTypeControllerTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        db_thread_(BrowserThread::DB) {}

  virtual void SetUp() OVERRIDE {
    EXPECT_CALL(service_, GetUserShare()).WillRepeatedly(
        Return((syncer::UserShare*)NULL));
    db_thread_.Start();
    profile_sync_factory_.reset(
        new StrictMock<ProfileSyncComponentsFactoryMock>());
    change_processor_ = new SharedChangeProcessorMock();

    // All of these are refcounted, so don't need to be released.
    dtc_mock_ = new StrictMock<NonUIDataTypeControllerMock>();
    non_ui_dtc_ =
        new NonUIDataTypeControllerFake(profile_sync_factory_.get(),
                                        &profile_,
                                        &service_,
                                        dtc_mock_.get());
  }

  virtual void TearDown() OVERRIDE {
    db_thread_.Stop();
  }

  void WaitForDTC() {
    WaitableEvent done(true, false);
    BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
       base::Bind(&SyncNonUIDataTypeControllerTest::SignalDone,
                  &done));
    done.TimedWait(TestTimeouts::action_timeout());
    if (!done.IsSignaled()) {
      ADD_FAILURE() << "Timed out waiting for DB thread to finish.";
    }
    MessageLoop::current()->RunUntilIdle();
  }

 protected:
  void SetStartExpectations() {
    EXPECT_CALL(*dtc_mock_, StartModels()).WillOnce(Return(true));
    EXPECT_CALL(model_load_callback_, Run(_, _));
    EXPECT_CALL(*profile_sync_factory_,
                CreateSharedChangeProcessor()).
        WillOnce(Return(change_processor_.get()));
  }

  void SetAssociateExpectations() {
    EXPECT_CALL(*change_processor_, Connect(_,_,_,_,_)).
        WillOnce(GetWeakPtrToSyncableService(&syncable_service_));
    EXPECT_CALL(*change_processor_, CryptoReadyIfNecessary()).
        WillOnce(Return(true));
    EXPECT_CALL(*change_processor_, ActivateDataType(_));
    EXPECT_CALL(*change_processor_, SyncModelHasUserCreatedNodes(_)).
        WillOnce(DoAll(SetArgumentPointee<0>(true), Return(true)));
    EXPECT_CALL(*change_processor_, GetSyncData(_)).
        WillOnce(Return(syncer::SyncError()));
    EXPECT_CALL(*change_processor_, GetSyncCount()).WillOnce(Return(0));
    EXPECT_CALL(*dtc_mock_, RecordAssociationTime(_));
  }

  void SetActivateExpectations(DataTypeController::StartResult result) {
    EXPECT_CALL(start_callback_, Run(result,_,_));
  }

  void SetStopExpectations() {
    EXPECT_CALL(*dtc_mock_, StopModels());
    EXPECT_CALL(*change_processor_, Disconnect()).WillOnce(Return(true));
    EXPECT_CALL(service_, DeactivateDataType(_));
  }

  void SetStartFailExpectations(DataTypeController::StartResult result) {
    EXPECT_CALL(*dtc_mock_, StopModels()).Times(AtLeast(1));
    if (DataTypeController::IsUnrecoverableResult(result))
      EXPECT_CALL(*dtc_mock_, RecordUnrecoverableError(_, _));
    EXPECT_CALL(*dtc_mock_, RecordStartFailure(result));
    EXPECT_CALL(start_callback_, Run(result,_,_));
  }

  void Start() {
    non_ui_dtc_->LoadModels(
        base::Bind(&ModelLoadCallbackMock::Run,
                   base::Unretained(&model_load_callback_)));
    non_ui_dtc_->StartAssociating(
        base::Bind(&StartCallbackMock::Run,
                   base::Unretained(&start_callback_)));
  }

  static void SignalDone(WaitableEvent* done) {
    done->Signal();
  }

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread db_thread_;
  ProfileMock profile_;
  scoped_ptr<ProfileSyncComponentsFactoryMock> profile_sync_factory_;
  StrictMock<ProfileSyncServiceMock> service_;
  StartCallbackMock start_callback_;
  ModelLoadCallbackMock model_load_callback_;
  // Must be destroyed after non_ui_dtc_.
  syncer::FakeSyncableService syncable_service_;
  scoped_refptr<NonUIDataTypeControllerFake> non_ui_dtc_;
  scoped_refptr<NonUIDataTypeControllerMock> dtc_mock_;
  scoped_refptr<SharedChangeProcessorMock> change_processor_;
  scoped_ptr<syncer::SyncChangeProcessor> saved_change_processor_;
};

TEST_F(SyncNonUIDataTypeControllerTest, StartOk) {
  SetStartExpectations();
  SetAssociateExpectations();
  SetActivateExpectations(DataTypeController::OK);
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_ui_dtc_->state());
  Start();
  WaitForDTC();
  EXPECT_EQ(DataTypeController::RUNNING, non_ui_dtc_->state());
}

TEST_F(SyncNonUIDataTypeControllerTest, StartFirstRun) {
  SetStartExpectations();
  EXPECT_CALL(*change_processor_, Connect(_,_,_,_,_)).
      WillOnce(GetWeakPtrToSyncableService(&syncable_service_));
  EXPECT_CALL(*change_processor_, CryptoReadyIfNecessary()).
      WillOnce(Return(true));
  EXPECT_CALL(*change_processor_, SyncModelHasUserCreatedNodes(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(false), Return(true)));
  EXPECT_CALL(*change_processor_, GetSyncData(_)).
      WillOnce(Return(syncer::SyncError()));
  EXPECT_CALL(*dtc_mock_, RecordAssociationTime(_));
  SetActivateExpectations(DataTypeController::OK_FIRST_RUN);
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_ui_dtc_->state());
  Start();
  WaitForDTC();
  EXPECT_EQ(DataTypeController::RUNNING, non_ui_dtc_->state());
}

// Start the DTC and have StartModels() return false.  Then, stop the
// DTC without finishing model startup.  It should stop cleanly.
TEST_F(SyncNonUIDataTypeControllerTest, AbortDuringStartModels) {
  EXPECT_CALL(*profile_sync_factory_,
              CreateSharedChangeProcessor()).
      WillOnce(Return(change_processor_.get()));
  EXPECT_CALL(*dtc_mock_, StartModels()).WillOnce(Return(false));
  EXPECT_CALL(*dtc_mock_, StopModels());
  EXPECT_CALL(model_load_callback_, Run(_, _));
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_ui_dtc_->state());
  non_ui_dtc_->LoadModels(
      base::Bind(&ModelLoadCallbackMock::Run,
                 base::Unretained(&model_load_callback_)));
  WaitForDTC();
  EXPECT_EQ(DataTypeController::MODEL_STARTING, non_ui_dtc_->state());
  non_ui_dtc_->Stop();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_ui_dtc_->state());
}

// Start the DTC and have MergeDataAndStartSyncing() return an error.
// The DTC should become disabled, and the DTC should still stop
// cleanly.
TEST_F(SyncNonUIDataTypeControllerTest, StartAssociationFailed) {
  SetStartExpectations();
  EXPECT_CALL(*change_processor_, Connect(_,_,_,_,_)).
      WillOnce(GetWeakPtrToSyncableService(&syncable_service_));
  EXPECT_CALL(*change_processor_, CryptoReadyIfNecessary()).
      WillOnce(Return(true));
  EXPECT_CALL(*change_processor_, SyncModelHasUserCreatedNodes(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(true), Return(true)));
  EXPECT_CALL(*change_processor_, GetSyncData(_)).
      WillOnce(Return(syncer::SyncError()));
  EXPECT_CALL(*dtc_mock_, RecordAssociationTime(_));
  SetStartFailExpectations(DataTypeController::ASSOCIATION_FAILED);
  // Set up association to fail with an association failed error.
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_ui_dtc_->state());
  syncable_service_.set_merge_data_and_start_syncing_error(
      syncer::SyncError(FROM_HERE,
                        "Sync Error", non_ui_dtc_->type()));
  Start();
  WaitForDTC();
  EXPECT_EQ(DataTypeController::DISABLED, non_ui_dtc_->state());
  non_ui_dtc_->Stop();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_ui_dtc_->state());
}

TEST_F(SyncNonUIDataTypeControllerTest,
       StartAssociationTriggersUnrecoverableError) {
  SetStartExpectations();
  SetStartFailExpectations(DataTypeController::UNRECOVERABLE_ERROR);
  // Set up association to fail with an unrecoverable error.
  EXPECT_CALL(*change_processor_, Connect(_,_,_,_,_)).
      WillOnce(GetWeakPtrToSyncableService(&syncable_service_));
  EXPECT_CALL(*change_processor_, CryptoReadyIfNecessary()).
      WillRepeatedly(Return(true));
  EXPECT_CALL(*change_processor_, SyncModelHasUserCreatedNodes(_)).
      WillRepeatedly(DoAll(SetArgumentPointee<0>(false), Return(false)));
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_ui_dtc_->state());
  Start();
  WaitForDTC();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_ui_dtc_->state());
}

TEST_F(SyncNonUIDataTypeControllerTest,
       StartAssociationCryptoNotReady) {
  SetStartExpectations();
  SetStartFailExpectations(DataTypeController::NEEDS_CRYPTO);
  // Set up association to fail with a NEEDS_CRYPTO error.
  EXPECT_CALL(*change_processor_, Connect(_,_,_,_,_)).
      WillOnce(GetWeakPtrToSyncableService(&syncable_service_));
  EXPECT_CALL(*change_processor_, CryptoReadyIfNecessary()).
      WillRepeatedly(Return(false));
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_ui_dtc_->state());
  Start();
  WaitForDTC();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_ui_dtc_->state());
}

// Trigger a Stop() call when we check if the model associator has user created
// nodes.
TEST_F(SyncNonUIDataTypeControllerTest, AbortDuringAssociation) {
  WaitableEvent wait_for_db_thread_pause(false, false);
  WaitableEvent pause_db_thread(false, false);

  SetStartExpectations();
  SetStartFailExpectations(DataTypeController::ABORTED);
  EXPECT_CALL(*change_processor_, Connect(_,_,_,_,_)).
      WillOnce(GetWeakPtrToSyncableService(&syncable_service_));
  EXPECT_CALL(*change_processor_, CryptoReadyIfNecessary()).
      WillOnce(Return(true));
  EXPECT_CALL(*change_processor_, SyncModelHasUserCreatedNodes(_)).
      WillOnce(DoAll(
          SignalEvent(&wait_for_db_thread_pause),
          WaitOnEvent(&pause_db_thread),
          SetArgumentPointee<0>(true),
          Return(true)));
  EXPECT_CALL(*change_processor_, GetSyncData(_)).
      WillOnce(
          Return(
              syncer::SyncError(FROM_HERE, "Disconnected.", AUTOFILL_PROFILE)));
  EXPECT_CALL(*change_processor_, Disconnect()).
      WillOnce(DoAll(SignalEvent(&pause_db_thread), Return(true)));
  EXPECT_CALL(service_, DeactivateDataType(_));
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_ui_dtc_->state());
  Start();
  wait_for_db_thread_pause.Wait();
  non_ui_dtc_->Stop();
  WaitForDTC();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_ui_dtc_->state());
}

// Start the DTC while the backend tasks are blocked. Then stop the DTC before
// the backend tasks get a chance to run. The DTC should have no interaction
// with the profile sync factory or profile sync service once stopped.
TEST_F(SyncNonUIDataTypeControllerTest, StartAfterSyncShutdown) {
  non_ui_dtc_->BlockBackendTasks();

  SetStartExpectations();
  // We don't expect StopSyncing to be called because local_service_ will never
  // have been set.
  EXPECT_CALL(*change_processor_, Disconnect()).WillOnce(Return(true));
  EXPECT_CALL(*dtc_mock_, StopModels());
  EXPECT_CALL(service_, DeactivateDataType(_));
  EXPECT_CALL(*dtc_mock_, RecordStartFailure(DataTypeController::ABORTED));
  EXPECT_CALL(start_callback_, Run(DataTypeController::ABORTED, _, _));
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_ui_dtc_->state());
  Start();
  non_ui_dtc_->Stop();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_ui_dtc_->state());
  Mock::VerifyAndClearExpectations(&profile_sync_factory_);
  Mock::VerifyAndClearExpectations(&service_);
  Mock::VerifyAndClearExpectations(change_processor_);
  Mock::VerifyAndClearExpectations(dtc_mock_);

  EXPECT_CALL(*change_processor_, Connect(_,_,_,_,_)).
      WillOnce(Return(base::WeakPtr<syncer::SyncableService>()));
  non_ui_dtc_->UnblockBackendTasks();
  WaitForDTC();
}

TEST_F(SyncNonUIDataTypeControllerTest, Stop) {
  SetStartExpectations();
  SetAssociateExpectations();
  SetActivateExpectations(DataTypeController::OK);
  SetStopExpectations();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_ui_dtc_->state());
  Start();
  WaitForDTC();
  EXPECT_EQ(DataTypeController::RUNNING, non_ui_dtc_->state());
  non_ui_dtc_->Stop();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_ui_dtc_->state());
}

// Start the DTC then block its backend tasks.  While its backend
// tasks are blocked, stop and start it again, then unblock its
// backend tasks.  The (delayed) running of the backend tasks from the
// stop after the restart shouldn't cause any problems.
TEST_F(SyncNonUIDataTypeControllerTest, StopStart) {
  SetStartExpectations();
  SetAssociateExpectations();
  SetActivateExpectations(DataTypeController::OK);
  SetStopExpectations();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_ui_dtc_->state());
  Start();
  WaitForDTC();
  EXPECT_EQ(DataTypeController::RUNNING, non_ui_dtc_->state());

  non_ui_dtc_->BlockBackendTasks();
  non_ui_dtc_->Stop();
  Mock::VerifyAndClearExpectations(&profile_sync_factory_);
  SetStartExpectations();
  SetAssociateExpectations();
  SetActivateExpectations(DataTypeController::OK);
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_ui_dtc_->state());
  Start();
  non_ui_dtc_->UnblockBackendTasks();

  WaitForDTC();
  EXPECT_EQ(DataTypeController::RUNNING, non_ui_dtc_->state());
}

TEST_F(SyncNonUIDataTypeControllerTest,
       OnSingleDatatypeUnrecoverableError) {
  SetStartExpectations();
  SetAssociateExpectations();
  SetActivateExpectations(DataTypeController::OK);
  EXPECT_CALL(*dtc_mock_, RecordUnrecoverableError(_, "Test"));
  EXPECT_CALL(service_, DisableBrokenDatatype(_,_,_)).WillOnce(
      InvokeWithoutArgs(non_ui_dtc_.get(),
                        &NonUIDataTypeController::Stop));
  SetStopExpectations();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_ui_dtc_->state());
  Start();
  WaitForDTC();
  EXPECT_EQ(DataTypeController::RUNNING, non_ui_dtc_->state());
  // This should cause non_ui_dtc_->Stop() to be called.
  BrowserThread::PostTask(BrowserThread::DB, FROM_HERE, base::Bind(
      &NonUIDataTypeControllerFake::
          OnSingleDatatypeUnrecoverableError,
      non_ui_dtc_.get(),
      FROM_HERE,
      std::string("Test")));
  WaitForDTC();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_ui_dtc_->state());
}

}  // namespace

}  // namespace browser_sync
