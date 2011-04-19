// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/synchronization/waitable_event.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/sync/glue/autofill_data_type_controller.h"
#include "chrome/browser/sync/glue/change_processor_mock.h"
#include "chrome/browser/sync/glue/model_associator_mock.h"
#include "chrome/browser/sync/profile_sync_factory_mock.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/browser/sync/profile_sync_test_util.h"
#include "chrome/browser/webdata/web_data_service.h"
#include "chrome/test/profile_mock.h"
#include "content/browser/browser_thread.h"
#include "content/common/notification_source.h"
#include "content/common/notification_type.h"
#include "testing/gmock/include/gmock/gmock.h"

using base::WaitableEvent;
using browser_sync::AutofillDataTypeController;
using browser_sync::ChangeProcessorMock;
using browser_sync::DataTypeController;
using browser_sync::ModelAssociatorMock;
using testing::_;
using testing::DoAll;
using testing::Invoke;
using testing::Return;
using testing::SetArgumentPointee;

namespace {

ACTION_P(WaitOnEvent, event) {
  event->Wait();
}

ACTION_P(SignalEvent, event) {
  event->Signal();
}

class StartCallback {
 public:
  MOCK_METHOD2(Run, void(DataTypeController::StartResult result,
      const tracked_objects::Location& location));
};

class PersonalDataManagerMock : public PersonalDataManager {
 public:
  MOCK_CONST_METHOD0(IsDataLoaded, bool());
};

class WebDataServiceFake : public WebDataService {
 public:
  explicit WebDataServiceFake(bool is_database_loaded)
      : is_database_loaded_(is_database_loaded) {}

  virtual bool IsDatabaseLoaded() {
    return is_database_loaded_;
  }
 private:
  bool is_database_loaded_;
};

class SignalEventTask : public Task {
 public:
  explicit SignalEventTask(WaitableEvent* done_event)
      : done_event_(done_event) {}

  virtual void Run() {
    done_event_->Signal();
  }

 private:
  WaitableEvent* done_event_;
};

class AutofillDataTypeControllerTest : public testing::Test {
 public:
  AutofillDataTypeControllerTest()
      : ui_thread_(BrowserThread::UI, &message_loop_),
        db_thread_(BrowserThread::DB) {}

  virtual void SetUp() {
    db_thread_.Start();
    web_data_service_ = new WebDataServiceFake(true);
    personal_data_manager_ = new PersonalDataManagerMock();
    autofill_dtc_ =
        new AutofillDataTypeController(&profile_sync_factory_,
                                       &profile_,
                                       &service_);
  }

  virtual void TearDown() {
    WaitForEmptyDBMessageLoop();
    db_thread_.Stop();
  }

 protected:
  void SetStartExpectations() {
    EXPECT_CALL(profile_, GetPersonalDataManager()).
        WillRepeatedly(Return(personal_data_manager_.get()));
    EXPECT_CALL(*(personal_data_manager_.get()), IsDataLoaded()).
        WillRepeatedly(Return(true));
    EXPECT_CALL(profile_, GetWebDataService(_)).
        WillOnce(Return(web_data_service_.get()));
  }

  void SetAssociateExpectations() {
    model_associator_ = new ModelAssociatorMock();
    change_processor_ = new ChangeProcessorMock();
    EXPECT_CALL(profile_sync_factory_,
                CreateAutofillSyncComponents(_, _, _, _)).
        WillOnce(Return(
            ProfileSyncFactory::SyncComponents(model_associator_,
                                               change_processor_)));
    EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary()).
        WillRepeatedly(Return(true));
    EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
        WillRepeatedly(DoAll(SetArgumentPointee<0>(true), Return(true)));
    EXPECT_CALL(*model_associator_, AssociateModels()).
        WillRepeatedly(Return(true));
    EXPECT_CALL(service_, ActivateDataType(_, _));
    EXPECT_CALL(*change_processor_, IsRunning()).WillRepeatedly(Return(true));
  }

  void SetStopExpectations() {
    EXPECT_CALL(service_, DeactivateDataType(_, _));
    EXPECT_CALL(*model_associator_, DisassociateModels());
  }

  void WaitForEmptyDBMessageLoop() {
    // Run a task through the DB message loop to ensure that
    // everything before it has been run.
    WaitableEvent done_event(false, false);
    BrowserThread::PostTask(BrowserThread::DB,
                            FROM_HERE,
                            new SignalEventTask(&done_event));
    done_event.Wait();
  }

  MessageLoopForUI message_loop_;
  BrowserThread ui_thread_;
  BrowserThread db_thread_;
  scoped_refptr<AutofillDataTypeController> autofill_dtc_;
  ProfileSyncFactoryMock profile_sync_factory_;
  ProfileMock profile_;
  scoped_refptr<PersonalDataManagerMock> personal_data_manager_;
  scoped_refptr<WebDataService> web_data_service_;
  ProfileSyncServiceMock service_;
  ModelAssociatorMock* model_associator_;
  ChangeProcessorMock* change_processor_;
  StartCallback start_callback_;
};

TEST_F(AutofillDataTypeControllerTest, StartPDMAndWDSReady) {
  SetStartExpectations();
  SetAssociateExpectations();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, autofill_dtc_->state());
  EXPECT_CALL(start_callback_, Run(DataTypeController::OK, _)).
      WillOnce(QuitUIMessageLoop());
  autofill_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  MessageLoop::current()->Run();
  EXPECT_EQ(DataTypeController::RUNNING, autofill_dtc_->state());
  SetStopExpectations();
  autofill_dtc_->Stop();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, autofill_dtc_->state());
}

TEST_F(AutofillDataTypeControllerTest, AbortWhilePDMStarting) {
  EXPECT_CALL(profile_, GetPersonalDataManager()).
      WillRepeatedly(Return(personal_data_manager_.get()));
  EXPECT_CALL(*(personal_data_manager_.get()), IsDataLoaded()).
      WillRepeatedly(Return(false));
  autofill_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  EXPECT_EQ(DataTypeController::MODEL_STARTING, autofill_dtc_->state());

  EXPECT_CALL(service_, DeactivateDataType(_, _)).Times(0);
  EXPECT_CALL(start_callback_, Run(DataTypeController::ABORTED, _));
  autofill_dtc_->Stop();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, autofill_dtc_->state());
}

TEST_F(AutofillDataTypeControllerTest, AbortWhileWDSStarting) {
  EXPECT_CALL(profile_, GetPersonalDataManager()).
      WillRepeatedly(Return(personal_data_manager_.get()));
  EXPECT_CALL(*(personal_data_manager_.get()), IsDataLoaded()).
      WillRepeatedly(Return(true));
  scoped_refptr<WebDataServiceFake> web_data_service_not_loaded(
      new WebDataServiceFake(false));
  EXPECT_CALL(profile_, GetWebDataService(_)).
      WillOnce(Return(web_data_service_not_loaded.get()));
  autofill_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  EXPECT_EQ(DataTypeController::MODEL_STARTING, autofill_dtc_->state());

  EXPECT_CALL(service_, DeactivateDataType(_, _)).Times(0);
  EXPECT_CALL(start_callback_, Run(DataTypeController::ABORTED, _));
  autofill_dtc_->Stop();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, autofill_dtc_->state());
}

TEST_F(AutofillDataTypeControllerTest, AbortWhileAssociatingNotActivated) {
  SetStartExpectations();

  model_associator_ = new ModelAssociatorMock();
  change_processor_ = new ChangeProcessorMock();
  EXPECT_CALL(profile_sync_factory_,
              CreateAutofillSyncComponents(_, _, _, _)).
      WillOnce(Return(
          ProfileSyncFactory::SyncComponents(model_associator_,
                                             change_processor_)));

  // Use the pause_db_thread WaitableEvent to pause the DB thread when
  // the model association process reaches the
  // SyncModelHasUserCreatedNodes method.  This lets us call Stop()
  // while model association is in progress.  When we call Stop(),
  // this will call the model associator's AbortAssociation() method
  // that signals the event and lets the DB thread continue.
  WaitableEvent pause_db_thread(false, false);
  WaitableEvent wait_for_db_thread_pause(false, false);
  EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary()).
      WillRepeatedly(Return(true));
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillOnce(DoAll(
          SignalEvent(&wait_for_db_thread_pause),
          WaitOnEvent(&pause_db_thread),
          SetArgumentPointee<0>(true),
          Return(false)));
  EXPECT_CALL(*model_associator_, AbortAssociation()).
      WillOnce(SignalEvent(&pause_db_thread));

  autofill_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  wait_for_db_thread_pause.Wait();
  EXPECT_EQ(DataTypeController::ASSOCIATING, autofill_dtc_->state());

  EXPECT_CALL(service_, DeactivateDataType(_, _)).Times(0);
  EXPECT_CALL(start_callback_, Run(DataTypeController::ABORTED, _));
  autofill_dtc_->Stop();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, autofill_dtc_->state());
}

TEST_F(AutofillDataTypeControllerTest, AbortWhileAssociatingActivated) {
  SetStartExpectations();

  model_associator_ = new ModelAssociatorMock();
  change_processor_ = new ChangeProcessorMock();
  EXPECT_CALL(profile_sync_factory_,
              CreateAutofillSyncComponents(_, _, _, _)).
      WillOnce(Return(
          ProfileSyncFactory::SyncComponents(model_associator_,
                                             change_processor_)));
  EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary()).
      WillRepeatedly(Return(true));
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillRepeatedly(DoAll(SetArgumentPointee<0>(true), Return(true)));
  EXPECT_CALL(*model_associator_, AssociateModels()).
      WillRepeatedly(Return(true));
  EXPECT_CALL(*change_processor_, IsRunning()).WillRepeatedly(Return(true));

  // The usage of pause_db_thread is similar as in the previous test,
  // but here we will wait until the DB thread calls
  // ActivateDataType() before  allowing it to continue.
  WaitableEvent pause_db_thread(false, false);
  WaitableEvent wait_for_db_thread_pause(false, false);
  EXPECT_CALL(service_, ActivateDataType(_, _)).
      WillOnce(DoAll(
          SignalEvent(&wait_for_db_thread_pause),
          WaitOnEvent(&pause_db_thread)));
  EXPECT_CALL(*model_associator_, AbortAssociation()).
      WillOnce(SignalEvent(&pause_db_thread));

  autofill_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  wait_for_db_thread_pause.Wait();
  EXPECT_EQ(DataTypeController::ASSOCIATING, autofill_dtc_->state());

  SetStopExpectations();
  EXPECT_CALL(start_callback_, Run(DataTypeController::ABORTED, _));
  autofill_dtc_->Stop();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, autofill_dtc_->state());
}

}  // namespace
