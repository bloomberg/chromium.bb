// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "base/test/test_timeouts.h"
#include "base/tracked_objects.h"
#include "chrome/browser/sync/glue/non_frontend_data_type_controller.h"
#include "chrome/browser/sync/glue/non_frontend_data_type_controller_mock.h"
#include "chrome/browser/sync/profile_sync_components_factory_mock.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/test/base/profile_mock.h"
#include "components/sync_driver/change_processor_mock.h"
#include "components/sync_driver/data_type_controller_mock.h"
#include "components/sync_driver/model_associator_mock.h"
#include "content/public/test/test_browser_thread.h"
#include "sync/internal_api/public/engine/model_safe_worker.h"

using base::WaitableEvent;
using syncer::GROUP_DB;
using browser_sync::NonFrontendDataTypeController;
using browser_sync::NonFrontendDataTypeControllerMock;
using sync_driver::ChangeProcessorMock;
using sync_driver::DataTypeController;
using sync_driver::ModelAssociatorMock;
using sync_driver::ModelLoadCallbackMock;
using sync_driver::StartCallbackMock;
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
      ProfileSyncService* sync_service,
      NonFrontendDataTypeControllerMock* mock)
      : NonFrontendDataTypeController(base::MessageLoopProxy::current(),
                                      base::Closure(),
                                      profile_sync_factory,
                                      profile,
                                      sync_service),
        mock_(mock) {}

  virtual syncer::ModelType type() const OVERRIDE { return syncer::BOOKMARKS; }
  virtual syncer::ModelSafeGroup model_safe_group() const OVERRIDE {
    return syncer::GROUP_DB;
  }

 private:
  virtual ~NonFrontendDataTypeControllerFake() {}

  virtual ProfileSyncComponentsFactory::SyncComponents
  CreateSyncComponents() OVERRIDE {
    return profile_sync_factory()->
            CreateBookmarkSyncComponents(profile_sync_service(), this);
  }

  virtual bool PostTaskOnBackendThread(
      const tracked_objects::Location& from_here,
      const base::Closure& task) OVERRIDE {
    return BrowserThread::PostTask(BrowserThread::DB, from_here, task);
  }

  // We mock the following methods because their default implementations do
  // nothing, but we still want to make sure they're called appropriately.
  virtual bool StartModels() OVERRIDE {
    return mock_->StartModels();
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
  virtual void DisconnectProcessor(
      sync_driver::ChangeProcessor* processor) OVERRIDE{
    mock_->DisconnectProcessor(processor);
  }

 private:
  NonFrontendDataTypeControllerMock* mock_;
};

class SyncNonFrontendDataTypeControllerTest : public testing::Test {
 public:
  SyncNonFrontendDataTypeControllerTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        db_thread_(BrowserThread::DB),
        service_(&profile_),
        model_associator_(NULL),
        change_processor_(NULL) {}

  virtual void SetUp() {
    db_thread_.Start();
    profile_sync_factory_.reset(new ProfileSyncComponentsFactoryMock());

    // All of these are refcounted, so don't need to be released.
    dtc_mock_ = new StrictMock<NonFrontendDataTypeControllerMock>();
    non_frontend_dtc_ =
        new NonFrontendDataTypeControllerFake(profile_sync_factory_.get(),
                                              &profile_,
                                              &service_,
                                              dtc_mock_.get());
  }

  virtual void TearDown() {
    if (non_frontend_dtc_->state() !=
        NonFrontendDataTypeController::NOT_RUNNING) {
      non_frontend_dtc_->Stop();
    }
    db_thread_.Stop();
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
    EXPECT_CALL(*dtc_mock_.get(), DisconnectProcessor(_));
    EXPECT_CALL(service_, DeactivateDataType(_));
    EXPECT_CALL(*model_associator_, DisassociateModels()).
                WillOnce(Return(syncer::SyncError()));
  }

  void SetStartFailExpectations(DataTypeController::ConfigureResult result) {
    if (DataTypeController::IsUnrecoverableResult(result))
      EXPECT_CALL(*dtc_mock_.get(), RecordUnrecoverableError(_, _));
    if (model_associator_) {
      EXPECT_CALL(*model_associator_, DisassociateModels()).
                  WillOnce(Return(syncer::SyncError()));
    }
    EXPECT_CALL(*dtc_mock_.get(), RecordStartFailure(result));
    EXPECT_CALL(start_callback_, Run(result, _, _));
  }

  static void SignalDone(WaitableEvent* done) {
    done->Signal();
  }

  void WaitForDTC() {
    WaitableEvent done(true, false);
    BrowserThread::PostTask(BrowserThread::DB, FROM_HERE,
        base::Bind(&SyncNonFrontendDataTypeControllerTest::SignalDone, &done));
    done.TimedWait(TestTimeouts::action_timeout());
    if (!done.IsSignaled()) {
      ADD_FAILURE() << "Timed out waiting for DB thread to finish.";
    }
    base::MessageLoop::current()->RunUntilIdle();
  }

  void Start() {
    non_frontend_dtc_->LoadModels(
        base::Bind(&ModelLoadCallbackMock::Run,
                   base::Unretained(&model_load_callback_)));
    non_frontend_dtc_->StartAssociating(
        base::Bind(&StartCallbackMock::Run,
                   base::Unretained(&start_callback_)));
  }

  base::MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread db_thread_;
  scoped_refptr<NonFrontendDataTypeControllerFake> non_frontend_dtc_;
  scoped_ptr<ProfileSyncComponentsFactoryMock> profile_sync_factory_;
  scoped_refptr<NonFrontendDataTypeControllerMock> dtc_mock_;
  ProfileMock profile_;
  ProfileSyncServiceMock service_;
  ModelAssociatorMock* model_associator_;
  ChangeProcessorMock* change_processor_;
  StartCallbackMock start_callback_;
  ModelLoadCallbackMock model_load_callback_;
};

TEST_F(SyncNonFrontendDataTypeControllerTest, StartOk) {
  SetStartExpectations();
  SetAssociateExpectations();
  SetActivateExpectations(DataTypeController::OK);
  SetStopExpectations();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_frontend_dtc_->state());
  Start();
  WaitForDTC();
  EXPECT_EQ(DataTypeController::RUNNING, non_frontend_dtc_->state());
}

TEST_F(SyncNonFrontendDataTypeControllerTest, StartFirstRun) {
  SetStartExpectations();
  EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary()).
      WillOnce(Return(true));
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(false), Return(true)));
  EXPECT_CALL(*model_associator_, AssociateModels(_, _)).
      WillOnce(Return(syncer::SyncError()));
  EXPECT_CALL(*dtc_mock_.get(), RecordAssociationTime(_));
  SetActivateExpectations(DataTypeController::OK_FIRST_RUN);
  SetStopExpectations();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_frontend_dtc_->state());
  Start();
  WaitForDTC();
  EXPECT_EQ(DataTypeController::RUNNING, non_frontend_dtc_->state());
}

TEST_F(SyncNonFrontendDataTypeControllerTest, StartAssociationFailed) {
  SetStartExpectations();
  EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary()).
      WillOnce(Return(true));
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillOnce(DoAll(SetArgumentPointee<0>(true), Return(true)));
  EXPECT_CALL(*model_associator_, AssociateModels(_, _)).
      WillOnce(
          Return(syncer::SyncError(FROM_HERE,
                                   syncer::SyncError::DATATYPE_ERROR,
                                   "Error",
                                   syncer::BOOKMARKS)));
  EXPECT_CALL(*dtc_mock_.get(), RecordAssociationTime(_));
  SetStartFailExpectations(DataTypeController::ASSOCIATION_FAILED);
  // Set up association to fail with an association failed error.
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_frontend_dtc_->state());
  Start();
  WaitForDTC();
  EXPECT_EQ(DataTypeController::DISABLED, non_frontend_dtc_->state());
}

TEST_F(SyncNonFrontendDataTypeControllerTest,
       StartAssociationTriggersUnrecoverableError) {
  SetStartExpectations();
  SetStartFailExpectations(DataTypeController::UNRECOVERABLE_ERROR);
  // Set up association to fail with an unrecoverable error.
  EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary()).
      WillRepeatedly(Return(true));
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillRepeatedly(DoAll(SetArgumentPointee<0>(false), Return(false)));
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_frontend_dtc_->state());
  Start();
  WaitForDTC();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_frontend_dtc_->state());
}

TEST_F(SyncNonFrontendDataTypeControllerTest, StartAssociationCryptoNotReady) {
  SetStartExpectations();
  SetStartFailExpectations(DataTypeController::NEEDS_CRYPTO);
  // Set up association to fail with a NEEDS_CRYPTO error.
  EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary()).
      WillRepeatedly(Return(false));
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_frontend_dtc_->state());
  Start();
  WaitForDTC();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_frontend_dtc_->state());
}

// Trigger a Stop() call when we check if the model associator has user created
// nodes.
TEST_F(SyncNonFrontendDataTypeControllerTest, AbortDuringAssociationInactive) {
  WaitableEvent wait_for_db_thread_pause(false, false);
  WaitableEvent pause_db_thread(false, false);

  SetStartExpectations();
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
  EXPECT_CALL(*model_associator_, AssociateModels(_, _)).
              WillOnce(Return(syncer::SyncError()));
  EXPECT_CALL(start_callback_, Run(DataTypeController::ABORTED,_,_));
  EXPECT_CALL(*dtc_mock_.get(),
              RecordStartFailure(DataTypeController::ABORTED));
  SetStopExpectations();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_frontend_dtc_->state());
  Start();
  wait_for_db_thread_pause.Wait();
  non_frontend_dtc_->Stop();
  WaitForDTC();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_frontend_dtc_->state());
}

// Same as above but abort during the Activate call.
TEST_F(SyncNonFrontendDataTypeControllerTest, AbortDuringAssociationActivated) {
  WaitableEvent wait_for_association_starts(false, false);
  WaitableEvent wait_for_dtc_stop(false, false);

  SetStartExpectations();
  EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary()).
      WillOnce(Return(true));
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillOnce(DoAll(
          SetArgumentPointee<0>(true),
          Return(true)));
  EXPECT_CALL(*model_associator_, AbortAssociation());
  EXPECT_CALL(*model_associator_, AssociateModels(_, _)).
      WillOnce(DoAll(
          SignalEvent(&wait_for_association_starts),
          WaitOnEvent(&wait_for_dtc_stop),
          Return(syncer::SyncError())));
  EXPECT_CALL(start_callback_, Run(DataTypeController::ABORTED,_,_));
  EXPECT_CALL(*dtc_mock_.get(),
              RecordStartFailure(DataTypeController::ABORTED));
  SetStopExpectations();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_frontend_dtc_->state());
  Start();
  wait_for_association_starts.Wait();
  non_frontend_dtc_->Stop();
  wait_for_dtc_stop.Signal();
  WaitForDTC();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_frontend_dtc_->state());
}

TEST_F(SyncNonFrontendDataTypeControllerTest, Stop) {
  SetStartExpectations();
  SetAssociateExpectations();
  SetActivateExpectations(DataTypeController::OK);
  SetStopExpectations();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_frontend_dtc_->state());
  Start();
  WaitForDTC();
  EXPECT_EQ(DataTypeController::RUNNING, non_frontend_dtc_->state());
  non_frontend_dtc_->Stop();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_frontend_dtc_->state());
}

// Disabled due to http://crbug.com/388367
TEST_F(SyncNonFrontendDataTypeControllerTest,
       DISABLED_OnSingleDatatypeUnrecoverableError) {
  SetStartExpectations();
  SetAssociateExpectations();
  SetActivateExpectations(DataTypeController::OK);
  EXPECT_CALL(*dtc_mock_.get(), RecordUnrecoverableError(_, "Test"));
  EXPECT_CALL(service_, DisableDatatype(_))
      .WillOnce(InvokeWithoutArgs(non_frontend_dtc_.get(),
                                  &NonFrontendDataTypeController::Stop));
  SetStopExpectations();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_frontend_dtc_->state());
  Start();
  WaitForDTC();
  EXPECT_EQ(DataTypeController::RUNNING, non_frontend_dtc_->state());
  // This should cause non_frontend_dtc_->Stop() to be called.
  BrowserThread::PostTask(BrowserThread::DB, FROM_HERE, base::Bind(
      &NonFrontendDataTypeControllerFake::OnSingleDatatypeUnrecoverableError,
      non_frontend_dtc_.get(),
      FROM_HERE,
      std::string("Test")));
  WaitForDTC();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, non_frontend_dtc_->state());
}
