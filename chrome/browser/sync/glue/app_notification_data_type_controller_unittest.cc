// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "base/tracked_objects.h"
#include "chrome/browser/extensions/app_notification_manager.h"
#include "chrome/browser/extensions/extension_service.h"
#include "chrome/browser/sync/glue/app_notification_data_type_controller.h"
#include "chrome/browser/sync/glue/data_type_controller_mock.h"
#include "chrome/browser/sync/glue/fake_generic_change_processor.h"
#include "chrome/browser/sync/profile_sync_components_factory_mock.h"
#include "chrome/browser/sync/profile_sync_service_mock.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/notification_service.h"
#include "content/test/test_browser_thread.h"
#include "sync/api/fake_syncable_service.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;
using testing::_;
using testing::DoAll;
using testing::InvokeWithoutArgs;
using testing::Return;
using testing::SetArgumentPointee;

namespace browser_sync {

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
  virtual ~TestAppNotificationDataTypeController() {}

  scoped_refptr<AppNotificationManager> manager_;
};

namespace {

ACTION(MakeSharedChangeProcessor) {
  return new SharedChangeProcessor();
}

ACTION_P(ReturnAndRelease, change_processor) {
  return change_processor->release();
}

class SyncAppNotificationDataTypeControllerTest
    : public testing::Test {
 public:
  SyncAppNotificationDataTypeControllerTest()
      : ui_thread_(BrowserThread::UI, &ui_loop_),
        file_thread_(BrowserThread::FILE),
        change_processor_(new FakeGenericChangeProcessor()) {
  }

  virtual void SetUp() {
    ASSERT_TRUE(BrowserThread::CurrentlyOn(BrowserThread::UI));
    file_thread_.Start();

    profile_.reset(new TestingProfile());
    profile_sync_factory_.reset(new ProfileSyncComponentsFactoryMock());
    app_notif_dtc_ = new TestAppNotificationDataTypeController(
        profile_sync_factory_.get(),
        profile_.get(),
        &service_);
    SetStartExpectations();
  }

  virtual void TearDown() {
    // Must be done before we pump the loop.
    syncable_service_.StopSyncing(syncable::APP_NOTIFICATIONS);
    app_notif_dtc_ = NULL;
    PumpLoop();
  }

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
                            MessageLoop::QuitClosure());
  }

  void InitAndLoadManager() {
    app_notif_dtc_->GetAppNotificationManager()->Init();
    WaitForFileThread();
  }

  void SetStartExpectations() {
    // Ownership gets passed to caller of CreateGenericChangeProcessor.
    EXPECT_CALL(*profile_sync_factory_,
                GetSyncableServiceForType(syncable::APP_NOTIFICATIONS)).
        WillOnce(Return(syncable_service_.AsWeakPtr()));
    EXPECT_CALL(*profile_sync_factory_, CreateSharedChangeProcessor()).
        WillOnce(MakeSharedChangeProcessor());
    EXPECT_CALL(*profile_sync_factory_, CreateGenericChangeProcessor(_, _, _)).
        WillOnce(ReturnAndRelease(&change_processor_));
  }

  void SetActivateExpectations() {
    EXPECT_CALL(service_, ActivateDataType(_, _, _));
  }

  void SetStopExpectations() {
    EXPECT_CALL(service_, DeactivateDataType(_));
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
  scoped_ptr<FakeGenericChangeProcessor> change_processor_;
  FakeSyncableService syncable_service_;
  StartCallbackMock start_callback_;
};

// When notification manager is ready, sync association should happen
// successfully.
TEST_F(SyncAppNotificationDataTypeControllerTest, StartManagerReady) {
  InitAndLoadManager();
  SetActivateExpectations();

  EXPECT_EQ(DataTypeController::NOT_RUNNING, app_notif_dtc_->state());
  EXPECT_CALL(start_callback_, Run(DataTypeController::OK, _));
  app_notif_dtc_->Start(
      base::Bind(&StartCallbackMock::Run, base::Unretained(&start_callback_)));
  EXPECT_EQ(DataTypeController::RUNNING, app_notif_dtc_->state());
}

// When notification manager is not ready, sync assocation should wait
// until loaded event is seen.
TEST_F(SyncAppNotificationDataTypeControllerTest, StartManagerNotReady) {
  SetActivateExpectations();
  EXPECT_CALL(start_callback_, Run(DataTypeController::OK, _));

  EXPECT_EQ(DataTypeController::NOT_RUNNING, app_notif_dtc_->state());
  app_notif_dtc_->Start(
      base::Bind(&StartCallbackMock::Run, base::Unretained(&start_callback_)));
  EXPECT_EQ(DataTypeController::MODEL_STARTING, app_notif_dtc_->state());

  // Unblock file thread and wait for it to finish all tasks.
    // Send the notification that the TemplateURLService has started.
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_APP_NOTIFICATION_MANAGER_LOADED,
      content::Source<AppNotificationManager>(
          app_notif_dtc_->GetAppNotificationManager()),
          content::NotificationService::NoDetails());
  EXPECT_EQ(DataTypeController::RUNNING, app_notif_dtc_->state());
  EXPECT_TRUE(syncable_service_.syncing());
}

TEST_F(SyncAppNotificationDataTypeControllerTest, StartFirstRun) {
  InitAndLoadManager();
  SetActivateExpectations();
  EXPECT_CALL(start_callback_, Run(DataTypeController::OK_FIRST_RUN, _));
  change_processor_->set_sync_model_has_user_created_nodes(false);

  app_notif_dtc_->Start(
      base::Bind(&StartCallbackMock::Run, base::Unretained(&start_callback_)));
  EXPECT_EQ(DataTypeController::RUNNING, app_notif_dtc_->state());
  EXPECT_TRUE(syncable_service_.syncing());
}

TEST_F(SyncAppNotificationDataTypeControllerTest, StartAssociationFailed) {
  InitAndLoadManager();
  EXPECT_CALL(start_callback_,
              Run(DataTypeController::ASSOCIATION_FAILED, _));
  syncable_service_.set_merge_data_and_start_syncing_error(
      SyncError(FROM_HERE, "Error", syncable::APP_NOTIFICATIONS));

  app_notif_dtc_->Start(
      base::Bind(&StartCallbackMock::Run, base::Unretained(&start_callback_)));
  EXPECT_EQ(DataTypeController::DISABLED, app_notif_dtc_->state());
  EXPECT_FALSE(syncable_service_.syncing());
}

TEST_F(SyncAppNotificationDataTypeControllerTest,
       StartAssociationTriggersUnrecoverableError) {
  InitAndLoadManager();
  EXPECT_CALL(start_callback_,
              Run(DataTypeController::UNRECOVERABLE_ERROR, _));
  // Set up association to fail with an unrecoverable error.
  change_processor_->set_sync_model_has_user_created_nodes_success(false);

  app_notif_dtc_->Start(
      base::Bind(&StartCallbackMock::Run, base::Unretained(&start_callback_)));
  EXPECT_EQ(DataTypeController::NOT_RUNNING, app_notif_dtc_->state());
  EXPECT_FALSE(syncable_service_.syncing());
}

TEST_F(SyncAppNotificationDataTypeControllerTest, Stop) {
  InitAndLoadManager();
  SetActivateExpectations();
  SetStopExpectations();
  EXPECT_CALL(start_callback_, Run(DataTypeController::OK, _));

  EXPECT_EQ(DataTypeController::NOT_RUNNING, app_notif_dtc_->state());
  EXPECT_FALSE(syncable_service_.syncing());
  app_notif_dtc_->Start(
      base::Bind(&StartCallbackMock::Run, base::Unretained(&start_callback_)));
  EXPECT_EQ(DataTypeController::RUNNING, app_notif_dtc_->state());
  EXPECT_TRUE(syncable_service_.syncing());
  app_notif_dtc_->Stop();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, app_notif_dtc_->state());
  EXPECT_FALSE(syncable_service_.syncing());
}

TEST_F(SyncAppNotificationDataTypeControllerTest, OnUnrecoverableError) {
  InitAndLoadManager();
  SetActivateExpectations();
  EXPECT_CALL(service_, OnUnrecoverableError(_, _)).
      WillOnce(InvokeWithoutArgs(app_notif_dtc_.get(),
                                 &AppNotificationDataTypeController::Stop));
  SetStopExpectations();

  EXPECT_CALL(start_callback_, Run(DataTypeController::OK, _));
  app_notif_dtc_->Start(
      base::Bind(&StartCallbackMock::Run, base::Unretained(&start_callback_)));
  EXPECT_EQ(DataTypeController::RUNNING, app_notif_dtc_->state());
  EXPECT_TRUE(syncable_service_.syncing());
  // This should cause app_notif_dtc_->Stop() to be called.
  app_notif_dtc_->OnUnrecoverableError(FROM_HERE, "Test");
  PumpLoop();
  EXPECT_EQ(DataTypeController::NOT_RUNNING, app_notif_dtc_->state());
  EXPECT_FALSE(syncable_service_.syncing());
}

}  // namespace
}  // namespace browser_sync
