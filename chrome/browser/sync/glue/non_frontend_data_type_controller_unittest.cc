// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/task.h"
#include "base/test/test_timeouts.h"
#include "base/tracked_objects.h"
#include "chrome/browser/sync/engine/model_safe_worker.h"
#include "chrome/browser/sync/glue/change_processor_mock.h"
#include "chrome/browser/sync/glue/data_type_controller_mock.h"
#include "chrome/browser/sync/glue/model_associator_mock.h"
#include "chrome/browser/sync/glue/non_frontend_data_type_controller.h"
#include "chrome/browser/sync/glue/non_frontend_data_type_controller_mock.h"
#include "chrome/browser/sync/profile_sync_components_factory_mock.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/test/base/profile_mock.h"
#include "content/test/test_browser_thread.h"

using base::WaitableEvent;
using browser_sync::ChangeProcessorMock;
using browser_sync::DataTypeController;
using browser_sync::GROUP_DB;
using browser_sync::NonFrontendDataTypeController;
using browser_sync::NonFrontendDataTypeControllerMock;
using browser_sync::ModelAssociatorMock;
using browser_sync::StartCallback;
using content::BrowserThread;
using testing::_;
using testing::DoAll;
using testing::InvokeWithoutArgs;
using testing::Return;
using testing::SetArgumentPointee;
using testing::StrictMock;

ACTION_P(WaitOnEvent, event) {
  event->Wait();
}

ACTION_P(SignalEvent, event) {
  event->Signal();
}

class NonFrontendDataTypeControllerFake : public NonFrontendDataTypeController {
 public:
  NonFrontendDataTypeControllerFake(
      ProfileSyncComponentsFactory* profile_sync_factory,
      Profile* profile,
      NonFrontendDataTypeControllerMock* mock)
      : NonFrontendDataTypeController(profile_sync_factory,
                                      profile),
        mock_(mock) {}

  virtual syncable::ModelType type() const { return syncable::BOOKMARKS; }
  virtual browser_sync::ModelSafeGroup model_safe_group() const {
    return GROUP_DB;
  }

 private:
  virtual void CreateSyncComponents() {
    ProfileSyncComponentsFactory::SyncComponents sync_components =
        profile_sync_factory()->
            CreateBookmarkSyncComponents(profile_sync_service(), this);
    set_model_associator(sync_components.model_associator);
    set_change_processor(sync_components.change_processor);
  }
  virtual bool StartAssociationAsync() {
    mock_->StartAssociationAsync();
    return BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
        base::Bind(&NonFrontendDataTypeControllerFake::StartAssociation, this));
  }
  virtual bool StopAssociationAsync() {
    mock_->StopAssociationAsync();
    return BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
        base::Bind(&NonFrontendDataTypeControllerFake::StopAssociation, this));
  }

  // We mock the following methods because their default implementations do
  // nothing, but we still want to make sure they're called appropriately.
  virtual bool StartModels() {
    return mock_->StartModels();
  }
  virtual void StopModels() {
    mock_->StopModels();
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
  NonFrontendDataTypeControllerMock* mock_;
};

class NonFrontendDataTypeControllerTest : public testing::Test {
 public:
  NonFrontendDataTypeControllerTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        db_thread_(BrowserThread::DB),
        model_associator_(NULL),
        change_processor_(NULL) {}

  virtual void SetUp() {
    EXPECT_CALL(profile_, GetProfileSyncService()).WillRepeatedly(
        Return(&service_));
    db_thread_.Start();
    profile_sync_factory_.reset(
        new StrictMock<ProfileSyncComponentsFactoryMock>());

    // Both of these are refcounted, so don't need to be released.
    dtc_mock_ = new StrictMock<NonFrontendDataTypeControllerMock>();
    non_frontend_dtc_ =
        new NonFrontendDataTypeControllerFake(profile_sync_factory_.get(),
                                           &profile_,
                                           dtc_mock_.get());
  }

  virtual void TearDown() {
    db_thread_.Stop();
  }

 protected:
  void SetStartExpectations() {
    EXPECT_CALL(*dtc_mock_, StartModels()).WillOnce(Return(true));
    model_associator_ = new ModelAssociatorMock();
    change_processor_ = new ChangeProcessorMock();
    EXPECT_CALL(*profile_sync_factory_, CreateBookmarkSyncComponents(_, _)).
        WillOnce(Return(ProfileSyncComponentsFactory::SyncComponents(
            model_associator_, change_processor_)));
  }

  void SetAssociateExpectations() {
    EXPECT_CALL(*dtc_mock_, StartAssociationAsync());
    EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary()).
        WillOnce(Return(true));
    EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
        WillOnce(DoAll(SetArgumentPointee<0>(true), Return(true)));
    EXPECT_CALL(*model_associator_, AssociateModels(_)).
        WillOnce(Return(true));
    EXPECT_CALL(*dtc_mock_, RecordAssociationTime(_));
  }

  void SetActivateExpectations(DataTypeController::StartResult result) {
    EXPECT_CALL(service_, ActivateDataType(_, _, _));
    EXPECT_CALL(start_callback_, Run(result,_));
  }

  void SetStopExpectations() {
    EXPECT_CALL(*dtc_mock_, StopAssociationAsync());
    EXPECT_CALL(*dtc_mock_, StopModels());
    EXPECT_CALL(service_, DeactivateDataType(_));
    EXPECT_CALL(*model_associator_, DisassociateModels(_));
  }

  void SetStartFailExpectations(DataTypeController::StartResult result) {
    EXPECT_CALL(*dtc_mock_, StopModels());
    EXPECT_CALL(*dtc_mock_, RecordStartFailure(result));
    EXPECT_CALL(start_callback_, Run(result,_));
  }

  static void SignalDone(WaitableEvent* done) {
    done->Signal();
  }

  void WaitForDTC() {
    WaitableEvent done(true, false);
    BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
        base::Bind(&NonFrontendDataTypeControllerTest::SignalDone, &done));
    done.TimedWait(base::TimeDelta::FromMilliseconds(
        TestTimeouts::action_timeout_ms()));
    if (!done.IsSignaled()) {
      ADD_FAILURE() << "Timed out waiting for DB thread to finish.";
    }
    MessageLoop::current()->RunAllPending();
  }

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread db_thread_;
  scoped_refptr<NonFrontendDataTypeControllerFake> non_frontend_dtc_;
  scoped_ptr<ProfileSyncComponentsFactoryMock> profile_sync_factory_;
  scoped_refptr<NonFrontendDataTypeControllerMock> dtc_mock_;
  ProfileMock profile_;
  ProfileSyncServiceMock service_;
  ModelAssociatorMock* model_associator_;
  ChangeProcessorMock* change_processor_;
  StartCallback start_callback_;
};

TEST_F(NonFrontendDataTypeControllerTest, StartOk) {
  SetStartExpectations();
  SetAssociateExpectations();
  SetActivateExpectations(DataTypeController::OK);
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_frontend_dtc_->state());
  non_frontend_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  WaitForDTC();
  EXPECT_EQ(DataTypeController::RUNNING, non_frontend_dtc_->state());
}

TEST_F(NonFrontendDataTypeControllerTest, StartFirstRun) {
  SetStartExpectations();
  EXPECT_CALL(*dtc_mock_, StartAssociationAsync());
  EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary()).
      WillOnce(Return(true));
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(false), Return(true)));
  EXPECT_CALL(*model_associator_, AssociateModels(_)).
      WillOnce(Return(true));
  EXPECT_CALL(*dtc_mock_, RecordAssociationTime(_));
  SetActivateExpectations(DataTypeController::OK_FIRST_RUN);
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_frontend_dtc_->state());
  non_frontend_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  WaitForDTC();
  EXPECT_EQ(DataTypeController::RUNNING, non_frontend_dtc_->state());
}

TEST_F(NonFrontendDataTypeControllerTest, AbortDuringStartModels) {
  EXPECT_CALL(*dtc_mock_, StartModels()).WillOnce(Return(false));
  SetStartFailExpectations(DataTypeController::ABORTED);
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_frontend_dtc_->state());
  non_frontend_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  WaitForDTC();
  EXPECT_EQ(DataTypeController::MODEL_STARTING, non_frontend_dtc_->state());
  non_frontend_dtc_->Stop();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_frontend_dtc_->state());
}

TEST_F(NonFrontendDataTypeControllerTest, StartAssociationFailed) {
  SetStartExpectations();
  EXPECT_CALL(*dtc_mock_, StartAssociationAsync());
  EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary()).
      WillOnce(Return(true));
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(true), Return(true)));
  EXPECT_CALL(*model_associator_, AssociateModels(_)).
      WillOnce(DoAll(browser_sync::SetSyncError(syncable::AUTOFILL),
                     Return(false)));
  EXPECT_CALL(*dtc_mock_, RecordAssociationTime(_));
  SetStartFailExpectations(DataTypeController::ASSOCIATION_FAILED);
  // Set up association to fail with an association failed error.
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_frontend_dtc_->state());
  non_frontend_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  WaitForDTC();
  EXPECT_EQ(DataTypeController::DISABLED, non_frontend_dtc_->state());
}

TEST_F(NonFrontendDataTypeControllerTest,
       StartAssociationTriggersUnrecoverableError) {
  SetStartExpectations();
  SetStartFailExpectations(DataTypeController::UNRECOVERABLE_ERROR);
  // Set up association to fail with an unrecoverable error.
  EXPECT_CALL(*dtc_mock_, StartAssociationAsync());
  EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary()).
      WillRepeatedly(Return(true));
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillRepeatedly(DoAll(SetArgumentPointee<0>(false), Return(false)));
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_frontend_dtc_->state());
  non_frontend_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  WaitForDTC();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_frontend_dtc_->state());
}

TEST_F(NonFrontendDataTypeControllerTest, StartAssociationCryptoNotReady) {
  SetStartExpectations();
  SetStartFailExpectations(DataTypeController::NEEDS_CRYPTO);
  // Set up association to fail with a NEEDS_CRYPTO error.
  EXPECT_CALL(*dtc_mock_, StartAssociationAsync());
  EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary()).
      WillRepeatedly(Return(false));
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_frontend_dtc_->state());
  non_frontend_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  WaitForDTC();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_frontend_dtc_->state());
}

// Trigger a Stop() call when we check if the model associator has user created
// nodes.
TEST_F(NonFrontendDataTypeControllerTest, AbortDuringAssociationInactive) {
  WaitableEvent wait_for_db_thread_pause(false, false);
  WaitableEvent pause_db_thread(false, false);

  SetStartExpectations();
  EXPECT_CALL(*dtc_mock_, StartAssociationAsync());
  EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary()).
      WillOnce(Return(true));
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillOnce(DoAll(
          SignalEvent(&wait_for_db_thread_pause),
          WaitOnEvent(&pause_db_thread),
          SetArgumentPointee<0>(true),
          Return(true)));
  EXPECT_CALL(*model_associator_, AbortAssociation()).WillOnce(
      SignalEvent(&pause_db_thread));
  EXPECT_CALL(*model_associator_, AssociateModels(_)).WillOnce(Return(true));
  EXPECT_CALL(*dtc_mock_, RecordAssociationTime(_));
  EXPECT_CALL(service_, ActivateDataType(_, _, _));
  EXPECT_CALL(start_callback_, Run(DataTypeController::ABORTED,_));
  EXPECT_CALL(*dtc_mock_, RecordStartFailure(DataTypeController::ABORTED));
  SetStopExpectations();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_frontend_dtc_->state());
  non_frontend_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  wait_for_db_thread_pause.Wait();
  non_frontend_dtc_->Stop();
  WaitForDTC();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_frontend_dtc_->state());
}

// Same as above but abort during the Activate call.
TEST_F(NonFrontendDataTypeControllerTest, AbortDuringAssociationActivated) {
  WaitableEvent wait_for_db_thread_pause(false, false);
  WaitableEvent pause_db_thread(false, false);

  SetStartExpectations();
  EXPECT_CALL(*dtc_mock_, StartAssociationAsync());
  EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary()).
      WillOnce(Return(true));
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillOnce(DoAll(
          SetArgumentPointee<0>(true),
          Return(true)));
  EXPECT_CALL(*model_associator_, AbortAssociation()).WillOnce(
      SignalEvent(&pause_db_thread));
  EXPECT_CALL(*model_associator_, AssociateModels(_)).
      WillOnce(Return(true));
  EXPECT_CALL(*dtc_mock_, RecordAssociationTime(_));
  EXPECT_CALL(service_, ActivateDataType(_, _, _)).WillOnce(DoAll(
      SignalEvent(&wait_for_db_thread_pause),
      WaitOnEvent(&pause_db_thread)));
  EXPECT_CALL(start_callback_, Run(DataTypeController::ABORTED,_));
  EXPECT_CALL(*dtc_mock_, RecordStartFailure(DataTypeController::ABORTED));
  SetStopExpectations();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_frontend_dtc_->state());
  non_frontend_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  wait_for_db_thread_pause.Wait();
  non_frontend_dtc_->Stop();
  WaitForDTC();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_frontend_dtc_->state());
}

TEST_F(NonFrontendDataTypeControllerTest, Stop) {
  SetStartExpectations();
  SetAssociateExpectations();
  SetActivateExpectations(DataTypeController::OK);
  SetStopExpectations();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_frontend_dtc_->state());
  non_frontend_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  WaitForDTC();
  EXPECT_EQ(DataTypeController::RUNNING, non_frontend_dtc_->state());
  non_frontend_dtc_->Stop();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_frontend_dtc_->state());
}

TEST_F(NonFrontendDataTypeControllerTest, OnUnrecoverableError) {
  SetStartExpectations();
  SetAssociateExpectations();
  SetActivateExpectations(DataTypeController::OK);
  EXPECT_CALL(*dtc_mock_, RecordUnrecoverableError(_, "Test"));
  EXPECT_CALL(service_, OnUnrecoverableError(_,_)).WillOnce(
      InvokeWithoutArgs(non_frontend_dtc_.get(),
          &NonFrontendDataTypeController::Stop));
  SetStopExpectations();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_frontend_dtc_->state());
  non_frontend_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  WaitForDTC();
  EXPECT_EQ(DataTypeController::RUNNING, non_frontend_dtc_->state());
  // This should cause non_frontend_dtc_->Stop() to be called.
  BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
      base::Bind(
          &NonFrontendDataTypeControllerFake::OnUnrecoverableError,
          non_frontend_dtc_.get(),
          FROM_HERE,
          std::string("Test")));
  WaitForDTC();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_frontend_dtc_->state());
}
