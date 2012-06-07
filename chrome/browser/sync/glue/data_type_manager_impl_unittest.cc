// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/data_type_manager_impl.h"

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "chrome/browser/sync/glue/backend_data_type_configurer.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/glue/fake_data_type_controller.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"
#include "content/public/test/mock_notification_observer.h"
#include "content/public/test/test_browser_thread.h"
#include "sync/internal_api/configure_reason.h"
#include "sync/internal_api/public/syncable/model_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

using syncable::ModelType;
using syncable::ModelTypeSet;
using syncable::ModelTypeToString;
using syncable::BOOKMARKS;
using syncable::APPS;
using syncable::PASSWORDS;
using syncable::PREFERENCES;
using testing::_;
using testing::Mock;
using testing::ResultOf;

// Fake BackendDataTypeConfigurer implementation that simply stores
// away the nigori state and callback passed into ConfigureDataTypes.
class FakeBackendDataTypeConfigurer : public BackendDataTypeConfigurer {
 public:
  FakeBackendDataTypeConfigurer() : last_nigori_state_(WITHOUT_NIGORI) {}
  virtual ~FakeBackendDataTypeConfigurer() {}

  virtual void ConfigureDataTypes(
      sync_api::ConfigureReason reason,
      ModelTypeSet types_to_add,
      ModelTypeSet types_to_remove,
      NigoriState nigori_state,
      base::Callback<void(ModelTypeSet)> ready_task,
      base::Callback<void()> retry_callback) OVERRIDE {
    last_nigori_state_ = nigori_state;
    last_ready_task_ = ready_task;
  }

  base::Callback<void(ModelTypeSet)> last_ready_task() const {
    return last_ready_task_;
  }

  NigoriState last_nigori_state() const {
    return last_nigori_state_;
  }

 private:
  base::Callback<void(ModelTypeSet)> last_ready_task_;
  NigoriState last_nigori_state_;
};


// Used by SetConfigureDoneExpectation.
DataTypeManager::ConfigureStatus GetStatus(
    const content::NotificationDetails& details) {
  const DataTypeManager::ConfigureResult* result =
      content::Details<const DataTypeManager::ConfigureResult>(
      details).ptr();
  return result->status;
}

// The actual test harness class, parametrized on NigoriState (i.e.,
// tests are run both configuring with nigori, and configuring
// without).
class SyncDataTypeManagerImplTest
    : public testing::TestWithParam<BackendDataTypeConfigurer::NigoriState> {
 public:
  SyncDataTypeManagerImplTest()
      : ui_thread_(content::BrowserThread::UI, &ui_loop_) {}

  virtual ~SyncDataTypeManagerImplTest() {
  }

 protected:
  virtual void SetUp() {
    registrar_.Add(&observer_,
                   chrome::NOTIFICATION_SYNC_CONFIGURE_START,
                   content::NotificationService::AllSources());
    registrar_.Add(&observer_,
                   chrome::NOTIFICATION_SYNC_CONFIGURE_DONE,
                   content::NotificationService::AllSources());
  }

  // A clearer name for the param accessor.
  BackendDataTypeConfigurer::NigoriState GetNigoriState() {
    return GetParam();
  }

  void SetConfigureStartExpectation() {
    EXPECT_CALL(
        observer_,
        Observe(int(chrome::NOTIFICATION_SYNC_CONFIGURE_START),
                _, _));
  }

  void SetConfigureDoneExpectation(DataTypeManager::ConfigureStatus status) {
    EXPECT_CALL(
        observer_,
        Observe(int(chrome::NOTIFICATION_SYNC_CONFIGURE_DONE), _,
        ResultOf(&GetStatus, status)));
  }

  // Configure the given DTM with the given desired types.
  void Configure(DataTypeManagerImpl* dtm,
                 const DataTypeManager::TypeSet& desired_types) {
    const sync_api::ConfigureReason kReason =
        sync_api::CONFIGURE_REASON_RECONFIGURATION;
    if (GetNigoriState() == BackendDataTypeConfigurer::WITH_NIGORI) {
      dtm->Configure(desired_types, kReason);
    } else {
      dtm->ConfigureWithoutNigori(desired_types, kReason);
    }
  }

  // Finish downloading for the given DTM.  Should be done only after
  // a call to Configure().
  void FinishDownload(const DataTypeManager& dtm,
                      ModelTypeSet failed_download_types) {
    EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm.state());
    EXPECT_EQ(GetNigoriState(), configurer_.last_nigori_state());
    ASSERT_FALSE(configurer_.last_ready_task().is_null());
    configurer_.last_ready_task().Run(failed_download_types);
  }

  // Adds a fake controller for the given type to |controllers_|.
  // Should be called only before setting up the DTM.
  void AddController(ModelType model_type) {
    controllers_[model_type] = new FakeDataTypeController(model_type);
  }

  // Gets the fake controller for the given type, which should have
  // been previously added via AddController().
  scoped_refptr<FakeDataTypeController> GetController(
      ModelType model_type) const {
    DataTypeController::TypeMap::const_iterator it =
        controllers_.find(model_type);
    if (it == controllers_.end()) {
      return NULL;
    }
    return make_scoped_refptr(
        static_cast<FakeDataTypeController*>(it->second.get()));
  }

  MessageLoopForUI ui_loop_;
  content::TestBrowserThread ui_thread_;
  DataTypeController::TypeMap controllers_;
  FakeBackendDataTypeConfigurer configurer_;
  content::MockNotificationObserver observer_;
  content::NotificationRegistrar registrar_;
};

// Set up a DTM with no controllers, configure it, finish downloading,
// and then stop it.
TEST_P(SyncDataTypeManagerImplTest, NoControllers) {
  DataTypeManagerImpl dtm(&configurer_, &controllers_);
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK);

  Configure(&dtm, ModelTypeSet());
  EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm.state());

  FinishDownload(dtm, ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm.state());

  dtm.Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}

// Set up a DTM with a single controller, configure it, finish
// downloading, finish starting the controller, and then stop the DTM.
TEST_P(SyncDataTypeManagerImplTest, ConfigureOne) {
  AddController(BOOKMARKS);

  DataTypeManagerImpl dtm(&configurer_, &controllers_);
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK);

  Configure(&dtm, ModelTypeSet(BOOKMARKS));
  EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm.state());

  FinishDownload(dtm, ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm.state());

  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm.state());

  dtm.Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}

// Set up a DTM with 2 controllers. configure it. One of them finishes loading
// after the  timeout. Make sure eventually all are configured.
TEST_P(SyncDataTypeManagerImplTest, ConfigureSlowLoadingType) {
  AddController(BOOKMARKS);
  AddController(APPS);

  GetController(BOOKMARKS)->SetDelayModelLoad();

  DataTypeManagerImpl dtm(&configurer_, &controllers_);
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::PARTIAL_SUCCESS);

  syncable::ModelTypeSet types;
  types.Put(BOOKMARKS);
  types.Put(APPS);

  Configure(&dtm, types);
  EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm.state());

  FinishDownload(dtm, ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm.state());

  base::OneShotTimer<ModelAssociationManager>* timer =
    dtm.GetModelAssociationManagerForTesting()->GetTimerForTesting();

  base::Closure task = timer->user_task();
  timer->Stop();
  task.Run();

  SetConfigureDoneExpectation(DataTypeManager::OK);
  GetController(APPS)->FinishStart(DataTypeController::OK);

  SetConfigureStartExpectation();
  GetController(BOOKMARKS)->SimulateModelLoadFinishing();

  FinishDownload(dtm, ModelTypeSet());
  GetController(BOOKMARKS)->SimulateModelLoadFinishing();

  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm.state());

  dtm.Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}


// Set up a DTM with a single controller, configure it, but stop it
// before finishing the download.  It should still be safe to run the
// download callback even after the DTM is stopped and destroyed.
TEST_P(SyncDataTypeManagerImplTest, ConfigureOneStopWhileDownloadPending) {
  AddController(BOOKMARKS);

  {
    DataTypeManagerImpl dtm(&configurer_, &controllers_);
    SetConfigureStartExpectation();
    SetConfigureDoneExpectation(DataTypeManager::ABORTED);

    Configure(&dtm, ModelTypeSet(BOOKMARKS));
    EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm.state());

    dtm.Stop();
    EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
  }

  configurer_.last_ready_task().Run(ModelTypeSet());
}

// Set up a DTM with a single controller, configure it, finish
// downloading, but stop the DTM before the controller finishes
// starting up.  It should still be safe to finish starting up the
// controller even after the DTM is stopped and destroyed.
TEST_P(SyncDataTypeManagerImplTest, ConfigureOneStopWhileStartingModel) {
  AddController(BOOKMARKS);

  {
    DataTypeManagerImpl dtm(&configurer_, &controllers_);
    SetConfigureStartExpectation();
    SetConfigureDoneExpectation(DataTypeManager::ABORTED);

    Configure(&dtm, ModelTypeSet(BOOKMARKS));
    EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm.state());

    FinishDownload(dtm, ModelTypeSet());
    EXPECT_EQ(DataTypeManager::CONFIGURING, dtm.state());

    dtm.Stop();
    EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
  }

  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
}

// Set up a DTM with a single controller, configure it, finish
// downloading, start the controller's model, but stop the DTM before
// the controller finishes starting up.  It should still be safe to
// finish starting up the controller even after the DTM is stopped and
// destroyed.
TEST_P(SyncDataTypeManagerImplTest, ConfigureOneStopWhileAssociating) {
  AddController(BOOKMARKS);
  {
    DataTypeManagerImpl dtm(&configurer_, &controllers_);
    SetConfigureStartExpectation();
    SetConfigureDoneExpectation(DataTypeManager::ABORTED);

    Configure(&dtm, ModelTypeSet(BOOKMARKS));
    EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm.state());

    FinishDownload(dtm, ModelTypeSet());
    EXPECT_EQ(DataTypeManager::CONFIGURING, dtm.state());

    dtm.Stop();
    EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
  }

  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
}

// Set up a DTM with a single controller.  Then:
//
//   1) Configure.
//   2) Finish the download for step 1.
//   3) Finish starting the controller with the NEEDS_CRYPTO status.
//   4) Configure again.
//   5) Finish the download for step 4.
//   6) Finish starting the controller successfully.
//   7) Stop the DTM.
TEST_P(SyncDataTypeManagerImplTest, OneWaitingForCrypto) {
  AddController(PASSWORDS);

  DataTypeManagerImpl dtm(&configurer_, &controllers_);
  SetConfigureStartExpectation();

  const ModelTypeSet types(PASSWORDS);

  // Step 1.
  Configure(&dtm, types);
  EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm.state());

  // Step 2.
  FinishDownload(dtm, ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm.state());

  // Step 3.
  GetController(PASSWORDS)->FinishStart(DataTypeController::NEEDS_CRYPTO);
  EXPECT_EQ(DataTypeManager::BLOCKED, dtm.state());

  Mock::VerifyAndClearExpectations(&observer_);
  SetConfigureDoneExpectation(DataTypeManager::OK);

  // Step 4.
  Configure(&dtm, types);
  EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm.state());

  // Step 5.
  FinishDownload(dtm, ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm.state());

  // Step 6.
  GetController(PASSWORDS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm.state());

  // Step 7.
  dtm.Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}

// Set up a DTM with two controllers.  Then:
//
//   1) Configure with first controller.
//   2) Finish the download for step 1.
//   3) Finish starting the first controller.
//   4) Configure with both controllers.
//   5) Finish the download for step 4.
//   6) Finish starting the second controller.
//   7) Stop the DTM.
TEST_P(SyncDataTypeManagerImplTest, ConfigureOneThenBoth) {
  AddController(BOOKMARKS);
  AddController(PREFERENCES);

  DataTypeManagerImpl dtm(&configurer_, &controllers_);
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK);

  // Step 1.
  Configure(&dtm, ModelTypeSet(BOOKMARKS));
  EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm.state());

  // Step 2.
  FinishDownload(dtm, ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm.state());

  // Step 3.
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm.state());

  Mock::VerifyAndClearExpectations(&observer_);
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK);

  // Step 4.
  Configure(&dtm, ModelTypeSet(BOOKMARKS, PREFERENCES));
  EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm.state());

  // Step 5.
  FinishDownload(dtm, ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm.state());

  // Step 6.
  GetController(PREFERENCES)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm.state());

  // Step 7.
  dtm.Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}

// Set up a DTM with two controllers.  Then:
//
//   1) Configure with first controller.
//   2) Finish the download for step 1.
//   3) Finish starting the first controller.
//   4) Configure with second controller.
//   5) Finish the download for step 4.
//   6) Finish starting the second controller.
//   7) Stop the DTM.
TEST_P(SyncDataTypeManagerImplTest, ConfigureOneThenSwitch) {
  AddController(BOOKMARKS);
  AddController(PREFERENCES);

  DataTypeManagerImpl dtm(&configurer_, &controllers_);
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK);

  // Step 1.
  Configure(&dtm, ModelTypeSet(BOOKMARKS));
  EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm.state());

  // Step 2.
  FinishDownload(dtm, ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm.state());

  // Step 3.
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm.state());

  Mock::VerifyAndClearExpectations(&observer_);
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK);

  // Step 4.
  Configure(&dtm, ModelTypeSet(PREFERENCES));
  EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm.state());

  // Step 5.
  FinishDownload(dtm, ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm.state());

  // Step 6.
  GetController(PREFERENCES)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm.state());

  // Step 7.
  dtm.Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}

// Set up a DTM with two controllers.  Then:
//
//   1) Configure with first controller.
//   2) Finish the download for step 1.
//   3) Configure with both controllers.
//   4) Finish starting the first controller.
//   5) Finish the download for step 3.
//   6) Finish starting the second controller.
//   7) Stop the DTM.
TEST_P(SyncDataTypeManagerImplTest, ConfigureWhileOneInFlight) {
  AddController(BOOKMARKS);
  AddController(PREFERENCES);

  DataTypeManagerImpl dtm(&configurer_, &controllers_);
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK);

  // Step 1.
  Configure(&dtm, ModelTypeSet(BOOKMARKS));
  EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm.state());

  // Step 2.
  FinishDownload(dtm, ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm.state());

  // Step 3.
  Configure(&dtm, ModelTypeSet(BOOKMARKS, PREFERENCES));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm.state());

  // Step 4.
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::BLOCKED, dtm.state());

  // Pump the loop to run the posted DTMI::ConfigureImpl() task from
  // DTMI::ProcessReconfigure() (triggered by FinishStart()).
  ui_loop_.RunAllPending();
  EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm.state());

  // Step 5.
  FinishDownload(dtm, ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm.state());

  // Step 6.
  GetController(PREFERENCES)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm.state());

  // Step 7.
  dtm.Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}

// Set up a DTM with one controller.  Then configure, finish
// downloading, and start the controller with an unrecoverable error.
// The unrecoverable error should cause the DTM to stop.
TEST_P(SyncDataTypeManagerImplTest, OneFailingController) {
  AddController(BOOKMARKS);

  DataTypeManagerImpl dtm(&configurer_, &controllers_);
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::UNRECOVERABLE_ERROR);

  Configure(&dtm, ModelTypeSet(BOOKMARKS));
  EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm.state());

  FinishDownload(dtm, ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm.state());

  GetController(BOOKMARKS)->FinishStart(
      DataTypeController::UNRECOVERABLE_ERROR);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}

// Set up a DTM with two controllers.  Then:
//
//   1) Configure with both controllers.
//   2) Finish the download for step 1.
//   3) Finish starting the first controller successfully.
//   4) Finish starting the second controller with an unrecoverable error.
//
// The failure from step 4 should cause the DTM to stop.
TEST_P(SyncDataTypeManagerImplTest, SecondControllerFails) {
  AddController(BOOKMARKS);
  AddController(PREFERENCES);

  DataTypeManagerImpl dtm(&configurer_, &controllers_);
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::UNRECOVERABLE_ERROR);

  // Step 1.
  Configure(&dtm, ModelTypeSet(BOOKMARKS, PREFERENCES));
  EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm.state());

  // Step 2.
  FinishDownload(dtm, ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm.state());

  // Step 3.
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm.state());

  // Step 4.
  GetController(PREFERENCES)->FinishStart(
      DataTypeController::UNRECOVERABLE_ERROR);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}

// Set up a DTM with two controllers.  Then:
//
//   1) Configure with both controllers.
//   2) Finish the download for step 1.
//   3) Finish starting the first controller with an association failure.
//   4) Finish starting the second controller successfully.
//   5) Stop the DTM.
//
// The association failure from step 3 should be ignored.
//
// TODO(akalin): Check that the data type that failed association is
// recorded in the CONFIGURE_DONE notification.
TEST_P(SyncDataTypeManagerImplTest, OneControllerFailsAssociation) {
  AddController(BOOKMARKS);
  AddController(PREFERENCES);

  DataTypeManagerImpl dtm(&configurer_, &controllers_);
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::PARTIAL_SUCCESS);

  // Step 1.
  Configure(&dtm, ModelTypeSet(BOOKMARKS, PREFERENCES));
  EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm.state());

  // Step 2.
  FinishDownload(dtm, ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm.state());

  // Step 3.
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm.state());

  // Step 4.
  GetController(PREFERENCES)->FinishStart(
      DataTypeController::ASSOCIATION_FAILED);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm.state());

  // Step 5.
  dtm.Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}

// Set up a DTM with two controllers.  Then:
//
//   1) Configure with first controller.
//   2) Configure with both controllers.
//   3) Finish the download for step 1.
//   4) Finish the download for step 2.
//   5) Finish starting both controllers.
//   6) Stop the DTM.
TEST_P(SyncDataTypeManagerImplTest, ConfigureWhileDownloadPending) {
  AddController(BOOKMARKS);
  AddController(PREFERENCES);

  DataTypeManagerImpl dtm(&configurer_, &controllers_);
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK);

  // Step 1.
  Configure(&dtm, ModelTypeSet(BOOKMARKS));
  EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm.state());

  // Step 2.
  Configure(&dtm, ModelTypeSet(BOOKMARKS, PREFERENCES));
  EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm.state());

  // Step 3.
  FinishDownload(dtm, ModelTypeSet());
  EXPECT_EQ(DataTypeManager::BLOCKED, dtm.state());

  // Pump the loop to run the posted DTMI::ConfigureImpl() task from
  // DTMI::ProcessReconfigure() (triggered by step 3).
  ui_loop_.RunAllPending();
  EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm.state());

  // Step 4.
  FinishDownload(dtm, ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm.state());

  // Step 5.
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm.state());
  GetController(PREFERENCES)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm.state());

  // Step 6.
  dtm.Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}

// Set up a DTM with two controllers.  Then:
//
//   1) Configure with first controller.
//   2) Configure with both controllers.
//   3) Finish the download for step 1 with a failed data type.
//   4) Finish the download for step 2 successfully.
//   5) Finish starting both controllers.
//   6) Stop the DTM.
//
// The failure from step 3 should be ignored since there's a
// reconfigure pending from step 2.
TEST_P(SyncDataTypeManagerImplTest, ConfigureWhileDownloadPendingWithFailure) {
  AddController(BOOKMARKS);
  AddController(PREFERENCES);

  DataTypeManagerImpl dtm(&configurer_, &controllers_);
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK);

  // Step 1.
  Configure(&dtm, ModelTypeSet(BOOKMARKS));
  EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm.state());

  // Step 2.
  Configure(&dtm, ModelTypeSet(BOOKMARKS, PREFERENCES));
  EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm.state());

  // Step 3.
  FinishDownload(dtm, ModelTypeSet(BOOKMARKS));
  EXPECT_EQ(DataTypeManager::BLOCKED, dtm.state());

  // Pump the loop to run the posted DTMI::ConfigureImpl() task from
  // DTMI::ProcessReconfigure() (triggered by step 3).
  ui_loop_.RunAllPending();
  EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm.state());

  // Step 4.
  FinishDownload(dtm, ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm.state());

  // Step 5.
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm.state());
  GetController(PREFERENCES)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm.state());

  // Step 6.
  dtm.Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm.state());
}

INSTANTIATE_TEST_CASE_P(
    WithoutNigori, SyncDataTypeManagerImplTest,
    ::testing::Values(BackendDataTypeConfigurer::WITHOUT_NIGORI));

INSTANTIATE_TEST_CASE_P(
    WithNigori, SyncDataTypeManagerImplTest,
    ::testing::Values(BackendDataTypeConfigurer::WITH_NIGORI));

}  // namespace browser_sync
