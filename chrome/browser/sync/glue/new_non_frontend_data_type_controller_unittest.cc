// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/new_non_frontend_data_type_controller.h"

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task.h"
#include "base/test/test_timeouts.h"
#include "base/tracked_objects.h"
#include "chrome/browser/sync/api/syncable_service_mock.h"
#include "chrome/browser/sync/engine/model_safe_worker.h"
#include "chrome/browser/sync/glue/data_type_controller_mock.h"
#include "chrome/browser/sync/glue/new_non_frontend_data_type_controller_mock.h"
#include "chrome/browser/sync/glue/shared_change_processor_mock.h"
#include "chrome/browser/sync/profile_sync_components_factory_mock.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/test/base/profile_mock.h"
#include "content/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::WaitableEvent;
using browser_sync::GenericChangeProcessor;
using browser_sync::SharedChangeProcessorMock;
using browser_sync::DataTypeController;
using browser_sync::GROUP_DB;
using browser_sync::NewNonFrontendDataTypeController;
using browser_sync::NewNonFrontendDataTypeControllerMock;
using browser_sync::StartCallback;
using content::BrowserThread;
using syncable::AUTOFILL_PROFILE;
using testing::_;
using testing::DoAll;
using testing::InvokeWithoutArgs;
using testing::Return;
using testing::SetArgumentPointee;
using testing::StrictMock;

namespace browser_sync {

namespace {

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

class NewNonFrontendDataTypeControllerFake
    : public NewNonFrontendDataTypeController {
 public:
  NewNonFrontendDataTypeControllerFake(
      ProfileSyncComponentsFactory* profile_sync_factory,
      Profile* profile,
      NewNonFrontendDataTypeControllerMock* mock)
      : NewNonFrontendDataTypeController(profile_sync_factory,
                                         profile),
        mock_(mock) {}

  virtual syncable::ModelType type() const OVERRIDE {
    return AUTOFILL_PROFILE;
  }
  virtual ModelSafeGroup model_safe_group() const OVERRIDE {
    return GROUP_DB;
  }

 protected:
  virtual base::WeakPtr<SyncableService>
      GetWeakPtrToSyncableService() const OVERRIDE {
    return profile_sync_factory()->GetAutofillProfileSyncableService(NULL);
  }
  virtual bool StartAssociationAsync() OVERRIDE {
    mock_->StartAssociationAsync();
    return BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
        base::Bind(&NewNonFrontendDataTypeControllerFake::StartAssociation,
                   this));
  }

  // We mock the following methods because their default implementations do
  // nothing, but we still want to make sure they're called appropriately.
  virtual bool StartModels() OVERRIDE {
    return mock_->StartModels();
  }
  virtual void StopModels() OVERRIDE {
    mock_->StopModels();
  }
  virtual void StopLocalServiceAsync() OVERRIDE {
    mock_->StopLocalServiceAsync();
    BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
        base::Bind(&NewNonFrontendDataTypeControllerFake::StopLocalService,
                   this));
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
  NewNonFrontendDataTypeControllerMock* mock_;
};

class NewNonFrontendDataTypeControllerTest : public testing::Test {
 public:
  NewNonFrontendDataTypeControllerTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        db_thread_(BrowserThread::DB) {}

  virtual void SetUp() OVERRIDE {
    EXPECT_CALL(profile_, GetProfileSyncService()).WillRepeatedly(
        Return(&service_));
    EXPECT_CALL(service_, GetUserShare()).WillRepeatedly(
        Return((sync_api::UserShare*)NULL));
    db_thread_.Start();
    profile_sync_factory_.reset(new ProfileSyncComponentsFactoryMock());
    change_processor_ = new SharedChangeProcessorMock();

    // Both of these are refcounted, so don't need to be released.
    dtc_mock_ = new StrictMock<NewNonFrontendDataTypeControllerMock>();
    new_non_frontend_dtc_ =
        new NewNonFrontendDataTypeControllerFake(profile_sync_factory_.get(),
                                                 &profile_,
                                                 dtc_mock_.get());
  }

  virtual void TearDown() OVERRIDE {
    db_thread_.Stop();
  }

  void WaitForDTC() {
    WaitableEvent done(true, false);
    BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
       base::Bind(&NewNonFrontendDataTypeControllerTest::SignalDone,
                  &done));
    done.TimedWait(base::TimeDelta::FromMilliseconds(
        TestTimeouts::action_timeout_ms()));
    if (!done.IsSignaled()) {
      ADD_FAILURE() << "Timed out waiting for DB thread to finish.";
    }
    MessageLoop::current()->RunAllPending();
  }

 protected:
  void SetStartExpectations() {
    EXPECT_CALL(*dtc_mock_, StartModels()).WillOnce(Return(true));
    EXPECT_CALL(*profile_sync_factory_,
                GetAutofillProfileSyncableService(_)).
        WillOnce(GetWeakPtrToSyncableService(&syncable_service_));
    EXPECT_CALL(*profile_sync_factory_,
                CreateSharedChangeProcessor()).
        WillOnce(Return(change_processor_.get()));
  }

  void SetAssociateExpectations() {
    EXPECT_CALL(*dtc_mock_, StartAssociationAsync());
    EXPECT_CALL(*change_processor_, Connect(_,_,_,_)).WillOnce(Return(true));
    EXPECT_CALL(*change_processor_, CryptoReadyIfNecessary(_)).
        WillOnce(Return(true));
    EXPECT_CALL(*change_processor_, SyncModelHasUserCreatedNodes(_,_)).
        WillOnce(DoAll(SetArgumentPointee<1>(true), Return(true)));
    EXPECT_CALL(*change_processor_, GetSyncDataForType(_,_)).
        WillOnce(Return(SyncError()));
    EXPECT_CALL(syncable_service_, MergeDataAndStartSyncing(_,_,_)).
        WillOnce(DoAll(SaveChangeProcessor(&saved_change_processor_),
                       Return(SyncError())));
    EXPECT_CALL(*dtc_mock_, RecordAssociationTime(_));
  }

  void SetActivateExpectations(DataTypeController::StartResult result) {
    EXPECT_CALL(service_, ActivateDataType(_, _, _));
    EXPECT_CALL(start_callback_, Run(result,_));
  }

  void SetStopExpectations() {
    EXPECT_CALL(*dtc_mock_, StopModels());
    EXPECT_CALL(*change_processor_, Disconnect()).WillOnce(Return(true));
    EXPECT_CALL(service_, DeactivateDataType(_));
    EXPECT_CALL(*dtc_mock_, StopLocalServiceAsync());
    EXPECT_CALL(syncable_service_, StopSyncing(_));
  }

  void SetStartFailExpectations(DataTypeController::StartResult result) {
    EXPECT_CALL(*dtc_mock_, StopLocalServiceAsync());
    EXPECT_CALL(syncable_service_, StopSyncing(_));
    EXPECT_CALL(*dtc_mock_, StopModels());
    EXPECT_CALL(*dtc_mock_, RecordStartFailure(result));
    EXPECT_CALL(start_callback_, Run(result,_));
  }

  static void SignalDone(WaitableEvent* done) {
    done->Signal();
  }

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread db_thread_;
  ProfileMock profile_;
  scoped_ptr<ProfileSyncComponentsFactoryMock> profile_sync_factory_;
  ProfileSyncServiceMock service_;
  StartCallback start_callback_;
  // Must be destroyed after new_non_frontend_dtc_.
  SyncableServiceMock syncable_service_;
  scoped_refptr<NewNonFrontendDataTypeControllerFake> new_non_frontend_dtc_;
  scoped_refptr<NewNonFrontendDataTypeControllerMock> dtc_mock_;
  scoped_refptr<SharedChangeProcessorMock> change_processor_;
  scoped_ptr<SyncChangeProcessor> saved_change_processor_;
};

TEST_F(NewNonFrontendDataTypeControllerTest, StartOk) {
  SetStartExpectations();
  SetAssociateExpectations();
  SetActivateExpectations(DataTypeController::OK);
  EXPECT_EQ(DataTypeController::NOT_RUNNING, new_non_frontend_dtc_->state());
  new_non_frontend_dtc_->Start(
      NewCallback(&start_callback_, &StartCallback::Run));
  WaitForDTC();
  EXPECT_EQ(DataTypeController::RUNNING, new_non_frontend_dtc_->state());
}

TEST_F(NewNonFrontendDataTypeControllerTest, StartFirstRun) {
  SetStartExpectations();
  EXPECT_CALL(*dtc_mock_, StartAssociationAsync());
  EXPECT_CALL(*change_processor_, Connect(_,_,_,_)).WillOnce(Return(true));
  EXPECT_CALL(*change_processor_, CryptoReadyIfNecessary(_)).
      WillOnce(Return(true));
  EXPECT_CALL(*change_processor_, SyncModelHasUserCreatedNodes(_,_)).
      WillOnce(DoAll(SetArgumentPointee<1>(false), Return(true)));
  EXPECT_CALL(*change_processor_, GetSyncDataForType(_,_)).
      WillOnce(Return(SyncError()));
  EXPECT_CALL(syncable_service_, MergeDataAndStartSyncing(_,_,_)).
    WillOnce(DoAll(SaveChangeProcessor(&saved_change_processor_),
                   Return(SyncError())));
  EXPECT_CALL(*dtc_mock_, RecordAssociationTime(_));
  SetActivateExpectations(DataTypeController::OK_FIRST_RUN);
  EXPECT_EQ(DataTypeController::NOT_RUNNING, new_non_frontend_dtc_->state());
  new_non_frontend_dtc_->Start(
      NewCallback(&start_callback_, &StartCallback::Run));
  WaitForDTC();
  EXPECT_EQ(DataTypeController::RUNNING, new_non_frontend_dtc_->state());
}

TEST_F(NewNonFrontendDataTypeControllerTest, AbortDuringStartModels) {
  EXPECT_CALL(*dtc_mock_, StartModels()).WillOnce(Return(false));
  EXPECT_CALL(*dtc_mock_, StopModels());
  EXPECT_CALL(*dtc_mock_, RecordStartFailure(DataTypeController::ABORTED));
  EXPECT_CALL(start_callback_, Run(DataTypeController::ABORTED,_));
  EXPECT_EQ(DataTypeController::NOT_RUNNING, new_non_frontend_dtc_->state());
  new_non_frontend_dtc_->Start(
      NewCallback(&start_callback_, &StartCallback::Run));
  WaitForDTC();
  EXPECT_EQ(DataTypeController::MODEL_STARTING, new_non_frontend_dtc_->state());
  new_non_frontend_dtc_->Stop();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, new_non_frontend_dtc_->state());
}

TEST_F(NewNonFrontendDataTypeControllerTest, StartAssociationFailed) {
  SetStartExpectations();
  EXPECT_CALL(*dtc_mock_, StartAssociationAsync());
  EXPECT_CALL(*change_processor_, Connect(_,_,_,_)).WillOnce(Return(true));
  EXPECT_CALL(*change_processor_, CryptoReadyIfNecessary(_)).
      WillOnce(Return(true));
  EXPECT_CALL(*change_processor_, SyncModelHasUserCreatedNodes(_,_)).
      WillOnce(DoAll(SetArgumentPointee<1>(true), Return(true)));
  EXPECT_CALL(*change_processor_, GetSyncDataForType(_,_)).
      WillOnce(Return(SyncError()));
  EXPECT_CALL(syncable_service_, MergeDataAndStartSyncing(_,_,_)).
    WillOnce(DoAll(SaveChangeProcessor(&saved_change_processor_),
                   Return(SyncError(FROM_HERE, "failed", AUTOFILL_PROFILE))));
  EXPECT_CALL(*dtc_mock_, RecordAssociationTime(_));
  SetStartFailExpectations(DataTypeController::ASSOCIATION_FAILED);
  // Set up association to fail with an association failed error.
  EXPECT_EQ(DataTypeController::NOT_RUNNING, new_non_frontend_dtc_->state());
  new_non_frontend_dtc_->Start(
      NewCallback(&start_callback_, &StartCallback::Run));
  WaitForDTC();
  EXPECT_EQ(DataTypeController::DISABLED, new_non_frontend_dtc_->state());
}

TEST_F(NewNonFrontendDataTypeControllerTest,
       StartAssociationTriggersUnrecoverableError) {
  SetStartExpectations();
  SetStartFailExpectations(DataTypeController::UNRECOVERABLE_ERROR);
  // Set up association to fail with an unrecoverable error.
  EXPECT_CALL(*dtc_mock_, StartAssociationAsync());
  EXPECT_CALL(*change_processor_, Connect(_,_,_,_)).WillOnce(Return(true));
  EXPECT_CALL(*change_processor_, CryptoReadyIfNecessary(_)).
      WillRepeatedly(Return(true));
  EXPECT_CALL(*change_processor_, SyncModelHasUserCreatedNodes(_,_)).
      WillRepeatedly(DoAll(SetArgumentPointee<1>(false), Return(false)));
  EXPECT_EQ(DataTypeController::NOT_RUNNING, new_non_frontend_dtc_->state());
  new_non_frontend_dtc_->Start(
      NewCallback(&start_callback_, &StartCallback::Run));
  WaitForDTC();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, new_non_frontend_dtc_->state());
}

TEST_F(NewNonFrontendDataTypeControllerTest, StartAssociationCryptoNotReady) {
  SetStartExpectations();
  SetStartFailExpectations(DataTypeController::NEEDS_CRYPTO);
  // Set up association to fail with a NEEDS_CRYPTO error.
  EXPECT_CALL(*dtc_mock_, StartAssociationAsync());
  EXPECT_CALL(*change_processor_, Connect(_,_,_,_)).WillOnce(Return(true));
  EXPECT_CALL(*change_processor_, CryptoReadyIfNecessary(_)).
      WillRepeatedly(Return(false));
  EXPECT_EQ(DataTypeController::NOT_RUNNING, new_non_frontend_dtc_->state());
  new_non_frontend_dtc_->Start(
      NewCallback(&start_callback_, &StartCallback::Run));
  WaitForDTC();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, new_non_frontend_dtc_->state());
}

// Trigger a Stop() call when we check if the model associator has user created
// nodes.
TEST_F(NewNonFrontendDataTypeControllerTest, AbortDuringAssociation) {
  WaitableEvent wait_for_db_thread_pause(false, false);
  WaitableEvent pause_db_thread(false, false);

  SetStartExpectations();
  SetStartFailExpectations(DataTypeController::ABORTED);
  EXPECT_CALL(*dtc_mock_, StartAssociationAsync());
  EXPECT_CALL(*change_processor_, Connect(_,_,_,_)).WillOnce(Return(true));
  EXPECT_CALL(*change_processor_, CryptoReadyIfNecessary(_)).
      WillOnce(Return(true));
  EXPECT_CALL(*change_processor_, SyncModelHasUserCreatedNodes(_,_)).
      WillOnce(DoAll(
          SignalEvent(&wait_for_db_thread_pause),
          WaitOnEvent(&pause_db_thread),
          SetArgumentPointee<1>(true),
          Return(true)));
  EXPECT_CALL(*change_processor_, GetSyncDataForType(_,_)).
      WillOnce(Return(SyncError(FROM_HERE, "Disconnected.", AUTOFILL_PROFILE)));
  EXPECT_CALL(*change_processor_, Disconnect()).
      WillOnce(DoAll(SignalEvent(&pause_db_thread), Return(true)));
  EXPECT_CALL(service_, DeactivateDataType(_));
  EXPECT_EQ(DataTypeController::NOT_RUNNING, new_non_frontend_dtc_->state());
  new_non_frontend_dtc_->Start(
      NewCallback(&start_callback_, &StartCallback::Run));
  wait_for_db_thread_pause.Wait();
  new_non_frontend_dtc_->Stop();
  WaitForDTC();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, new_non_frontend_dtc_->state());
}

TEST_F(NewNonFrontendDataTypeControllerTest, Stop) {
  SetStartExpectations();
  SetAssociateExpectations();
  SetActivateExpectations(DataTypeController::OK);
  SetStopExpectations();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, new_non_frontend_dtc_->state());
  new_non_frontend_dtc_->Start(
      NewCallback(&start_callback_, &StartCallback::Run));
  WaitForDTC();
  EXPECT_EQ(DataTypeController::RUNNING, new_non_frontend_dtc_->state());
  new_non_frontend_dtc_->Stop();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, new_non_frontend_dtc_->state());
}

TEST_F(NewNonFrontendDataTypeControllerTest, OnUnrecoverableError) {
  SetStartExpectations();
  SetAssociateExpectations();
  SetActivateExpectations(DataTypeController::OK);
  EXPECT_CALL(*dtc_mock_, RecordUnrecoverableError(_, "Test"));
  EXPECT_CALL(service_, OnUnrecoverableError(_,_)).WillOnce(
      InvokeWithoutArgs(new_non_frontend_dtc_.get(),
                        &NewNonFrontendDataTypeController::Stop));
  SetStopExpectations();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, new_non_frontend_dtc_->state());
  new_non_frontend_dtc_->Start(
      NewCallback(&start_callback_, &StartCallback::Run));
  WaitForDTC();
  EXPECT_EQ(DataTypeController::RUNNING, new_non_frontend_dtc_->state());
  // This should cause new_non_frontend_dtc_->Stop() to be called.
  BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      base::Bind(
          &NewNonFrontendDataTypeControllerFake::OnUnrecoverableError,
          new_non_frontend_dtc_.get(),
          FROM_HERE,
          std::string("Test")));
  WaitForDTC();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, new_non_frontend_dtc_->state());
}

}  // namespace

}  // namespace browser_sync
