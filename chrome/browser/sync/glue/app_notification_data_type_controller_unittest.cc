// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/task.h"
#include "base/tracked_objects.h"
#include "chrome/browser/extensions/app_notification_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/sync/glue/app_notification_data_type_controller.h"
#include "chrome/browser/sync/glue/change_processor_mock.h"
#include "chrome/browser/sync/glue/data_type_controller_mock.h"
#include "chrome/browser/sync/glue/model_associator_mock.h"
#include "chrome/browser/sync/profile_sync_components_factory_mock.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_service.h"
#include "content/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

using browser_sync::AppNotificationDataTypeController;
using browser_sync::ChangeProcessorMock;
using browser_sync::DataTypeController;
using browser_sync::ModelAssociatorMock;
using browser_sync::StartCallback;
using content::BrowserThread;
using testing::_;
using testing::DoAll;
using testing::InvokeWithoutArgs;
using testing::Return;
using testing::SetArgumentPointee;

class TestAppNotificationDataTypeController
    : public AppNotificationDataTypeController {
 public:
  TestAppNotificationDataTypeController(
      ProfileSyncComponentsFactory* profile_sync_factory,
      Profile* profile,
      ProfileSyncService* sync_service)
      : AppNotificationDataTypeController(profile_sync_factory,
                                          profile,
                                          sync_service),
        manager_(new AppNotificationManager(profile_)) {
  }

  virtual AppNotificationManager* GetAppNotificationManager() {
    return manager_.get();
  }

 private:
  scoped_refptr<AppNotificationManager> manager_;
};

class AppNotificationDataTypeControllerTest
    : public testing::Test {
 public:
  AppNotificationDataTypeControllerTest()
      : ui_thread_(BrowserThread::UI, &ui_loop_),
        file_thread_(BrowserThread::FILE) {
  }

  virtual void SetUp() {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    file_thread_.Start();

    profile_.reset(new TestingProfile());
    model_associator_ = new ModelAssociatorMock();
    change_processor_ = new ChangeProcessorMock();
    profile_sync_factory_.reset(new ProfileSyncComponentsFactoryMock(
        model_associator_, change_processor_));
    app_notif_dtc_ = new TestAppNotificationDataTypeController(
        profile_sync_factory_.get(),
        profile_.get(),
        &service_);
  }

  virtual void TearDown() { }

 protected:
  // Waits until the file thread executes all tasks queued up so far.
  static void WaitForFileThread() {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    // Post a task on the file thread that will post a task back on the
    // UI thread to quit the loop.
    BrowserThread::PostTask(BrowserThread::FILE,
                            FROM_HERE,
                            base::Bind(&PostQuitToUIThread));
    // Now run the message loop until quit is called.
    MessageLoop::current()->Run();
  }

  // Posts quit task on the UI thread.
  static void PostQuitToUIThread() {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::FILE));
    BrowserThread::PostTask(BrowserThread::UI,
                            FROM_HERE,
                            new MessageLoop::QuitTask());
  }

  void InitAndLoadManager() {
    app_notif_dtc_->GetAppNotificationManager()->Init();
    WaitForFileThread();
  }

  void SetAssociateExpectations() {
    EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary()).
        WillRepeatedly(Return(true));
    EXPECT_CALL(*profile_sync_factory_,
        CreateAppNotificationSyncComponents(_, _));
    EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
        WillRepeatedly(DoAll(SetArgumentPointee<0>(true), Return(true)));
    EXPECT_CALL(*model_associator_, AssociateModels(_)).
        WillRepeatedly(Return(true));
    EXPECT_CALL(service_, ActivateDataType(_, _, _));
  }

  void SetStopExpectations() {
    EXPECT_CALL(service_, DeactivateDataType(_));
    EXPECT_CALL(*model_associator_, DisassociateModels(_));
  }

  void PumpLoop() {
    ui_loop_.RunAllPending();
  }

  MessageLoop ui_loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;
  scoped_ptr<TestingProfile> profile_;
  scoped_refptr<TestAppNotificationDataTypeController> app_notif_dtc_;
  scoped_ptr<ProfileSyncComponentsFactoryMock> profile_sync_factory_;
  ProfileSyncServiceMock service_;
  ModelAssociatorMock* model_associator_;
  ChangeProcessorMock* change_processor_;
  StartCallback start_callback_;
};

// When notification manager is ready, sync assocation should happen
// successfully.
TEST_F(AppNotificationDataTypeControllerTest, StartManagerReady) {
  InitAndLoadManager();

  EXPECT_EQ(DataTypeController::NOT_RUNNING, app_notif_dtc_->state());
  SetAssociateExpectations();
  EXPECT_CALL(start_callback_, Run(DataTypeController::OK, _));
  app_notif_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  EXPECT_EQ(DataTypeController::RUNNING, app_notif_dtc_->state());
}

// When notification manager is not ready, sync assocation should wait
// until loaded event is seen.
TEST_F(AppNotificationDataTypeControllerTest, StartManagerNotReady) {
  EXPECT_EQ(DataTypeController::NOT_RUNNING, app_notif_dtc_->state());
  SetAssociateExpectations();
  EXPECT_CALL(start_callback_, Run(DataTypeController::OK, _));
  app_notif_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  EXPECT_EQ(DataTypeController::MODEL_STARTING, app_notif_dtc_->state());

  // Unblock file thread and wait for it to finish all tasks.
    // Send the notification that the TemplateURLService has started.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_APP_NOTIFICATION_MANAGER_LOADED,
      content::Source<AppNotificationManager>(
          app_notif_dtc_->GetAppNotificationManager()),
          content::NotificationService::NoDetails());
  EXPECT_EQ(DataTypeController::RUNNING, app_notif_dtc_->state());
}

TEST_F(AppNotificationDataTypeControllerTest, StartFirstRun) {
  InitAndLoadManager();
  SetAssociateExpectations();
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillRepeatedly(DoAll(SetArgumentPointee<0>(false), Return(true)));
  EXPECT_CALL(start_callback_, Run(DataTypeController::OK_FIRST_RUN, _));
  app_notif_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
}

TEST_F(AppNotificationDataTypeControllerTest, StartOk) {
  InitAndLoadManager();
  SetAssociateExpectations();
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillRepeatedly(DoAll(SetArgumentPointee<0>(true), Return(true)));
  EXPECT_CALL(start_callback_, Run(DataTypeController::OK, _));
  app_notif_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
}

TEST_F(AppNotificationDataTypeControllerTest, StartAssociationFailed) {
  InitAndLoadManager();
  EXPECT_CALL(*profile_sync_factory_,
      CreateAppNotificationSyncComponents(_, _));
  EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary()).
      WillRepeatedly(Return(true));
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillRepeatedly(DoAll(SetArgumentPointee<0>(true), Return(true)));
  EXPECT_CALL(*model_associator_, AssociateModels(_)).
      WillRepeatedly(DoAll(
          browser_sync::SetSyncError(syncable::APP_NOTIFICATIONS),
          Return(false)));

  EXPECT_CALL(start_callback_, Run(DataTypeController::ASSOCIATION_FAILED, _));
  app_notif_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  EXPECT_EQ(DataTypeController::DISABLED, app_notif_dtc_->state());
}

TEST_F(AppNotificationDataTypeControllerTest,
       StartAssociationTriggersUnrecoverableError) {
  InitAndLoadManager();
  // Set up association to fail with an unrecoverable error.
  EXPECT_CALL(*profile_sync_factory_,
      CreateAppNotificationSyncComponents(_, _));
  EXPECT_CALL(*model_associator_, CryptoReadyIfNecessary()).
      WillRepeatedly(Return(true));
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillRepeatedly(DoAll(SetArgumentPointee<0>(false), Return(false)));
  EXPECT_CALL(start_callback_, Run(DataTypeController::UNRECOVERABLE_ERROR, _));
  app_notif_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  EXPECT_EQ(DataTypeController::NOT_RUNNING, app_notif_dtc_->state());
}

TEST_F(AppNotificationDataTypeControllerTest, Stop) {
  InitAndLoadManager();
  SetAssociateExpectations();
  SetStopExpectations();

  EXPECT_EQ(DataTypeController::NOT_RUNNING, app_notif_dtc_->state());

  EXPECT_CALL(start_callback_, Run(DataTypeController::OK, _));
  app_notif_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  EXPECT_EQ(DataTypeController::RUNNING, app_notif_dtc_->state());
  app_notif_dtc_->Stop();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, app_notif_dtc_->state());
}

TEST_F(AppNotificationDataTypeControllerTest, OnUnrecoverableError) {
  InitAndLoadManager();
  SetAssociateExpectations();
  EXPECT_CALL(*model_associator_, SyncModelHasUserCreatedNodes(_)).
      WillRepeatedly(DoAll(SetArgumentPointee<0>(true), Return(true)));
  EXPECT_CALL(service_, OnUnrecoverableError(_, _)).
      WillOnce(InvokeWithoutArgs(app_notif_dtc_.get(),
                                 &AppNotificationDataTypeController::Stop));
  SetStopExpectations();

  EXPECT_CALL(start_callback_, Run(DataTypeController::OK, _));
  app_notif_dtc_->Start(NewCallback(&start_callback_, &StartCallback::Run));
  // This should cause app_notif_dtc_->Stop() to be called.
  app_notif_dtc_->OnUnrecoverableError(FROM_HERE, "Test");
  PumpLoop();
}
