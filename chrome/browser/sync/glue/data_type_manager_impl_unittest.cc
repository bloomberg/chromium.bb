// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/glue/data_type_manager_impl.h"

#include "base/compiler_specific.h"
#include "base/message_loop.h"
#include "chrome/browser/sync/glue/backend_data_type_configurer.h"
#include "chrome/browser/sync/glue/data_type_controller.h"
#include "chrome/browser/sync/glue/data_type_manager_observer.h"
#include "chrome/browser/sync/glue/fake_data_type_controller.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/test/test_browser_thread.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/configure_reason.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace browser_sync {

using syncer::ModelType;
using syncer::ModelTypeSet;
using syncer::ModelTypeToString;
using syncer::BOOKMARKS;
using syncer::APPS;
using syncer::PASSWORDS;
using syncer::PREFERENCES;
using syncer::NIGORI;
using testing::_;
using testing::Mock;
using testing::ResultOf;

// Fake BackendDataTypeConfigurer implementation that simply stores away the
// callback passed into ConfigureDataTypes.
class FakeBackendDataTypeConfigurer : public BackendDataTypeConfigurer {
 public:
  FakeBackendDataTypeConfigurer() {}
  virtual ~FakeBackendDataTypeConfigurer() {}

  virtual void ConfigureDataTypes(
      syncer::ConfigureReason reason,
      const DataTypeConfigStateMap& config_state_map,
      const base::Callback<void(ModelTypeSet)>& ready_task,
      const base::Callback<void()>& retry_callback) OVERRIDE {
    last_ready_task_ = ready_task;
  }

  base::Callback<void(ModelTypeSet)> last_ready_task() const {
    return last_ready_task_;
  }

 private:
  base::Callback<void(ModelTypeSet)> last_ready_task_;
};

// Mock DataTypeManagerObserver implementation.
class DataTypeManagerObserverMock : public DataTypeManagerObserver {
 public:
  DataTypeManagerObserverMock() {}
  virtual ~DataTypeManagerObserverMock() {}

  MOCK_METHOD0(OnConfigureBlocked, void());
  MOCK_METHOD1(OnConfigureDone,
               void(const browser_sync::DataTypeManager::ConfigureResult&));
  MOCK_METHOD0(OnConfigureRetry, void());
  MOCK_METHOD0(OnConfigureStart, void());
};

// Used by SetConfigureDoneExpectation.
DataTypeManager::ConfigureStatus GetStatus(
    const DataTypeManager::ConfigureResult& result) {
  return result.status;
}

// The actual test harness class, parametrized on nigori state (i.e., tests are
// run both configuring with nigori, and configuring without).
class SyncDataTypeManagerImplTest : public testing::Test {
 public:
  SyncDataTypeManagerImplTest()
      : ui_thread_(content::BrowserThread::UI, &ui_loop_) {}

  virtual ~SyncDataTypeManagerImplTest() {
  }

 protected:
  virtual void SetUp() {
   dtm_.reset(
       new DataTypeManagerImpl(
           syncer::WeakHandle<syncer::DataTypeDebugInfoListener>(),
           &configurer_,
           &controllers_,
           &observer_,
           NULL));
  }

  void SetConfigureStartExpectation() {
    EXPECT_CALL(observer_, OnConfigureStart());
  }

  void SetConfigureBlockedExpectation() {
    EXPECT_CALL(observer_, OnConfigureBlocked());
  }

  void SetConfigureDoneExpectation(DataTypeManager::ConfigureStatus status) {
    EXPECT_CALL(observer_, OnConfigureDone(ResultOf(&GetStatus, status)));
  }

  // Configure the given DTM with the given desired types.
  void Configure(DataTypeManagerImpl* dtm,
                 const DataTypeManager::TypeSet& desired_types) {
    dtm->Configure(desired_types, syncer::CONFIGURE_REASON_RECONFIGURATION);
  }

  // Finish downloading for the given DTM. Should be done only after
  // a call to Configure().
  void FinishDownload(const DataTypeManager& dtm,
                      ModelTypeSet failed_download_types) {
    EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm.state());
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
  DataTypeManagerObserverMock observer_;
  scoped_ptr<DataTypeManagerImpl> dtm_;
};

// Set up a DTM with no controllers, configure it, finish downloading,
// and then stop it.
TEST_F(SyncDataTypeManagerImplTest, NoControllers) {
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK);

  Configure(dtm_.get(), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm_->state());

  FinishDownload(*dtm_, ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());

  dtm_->Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
}

// Set up a DTM with a single controller, configure it, finish
// downloading, finish starting the controller, and then stop the DTM.
TEST_F(SyncDataTypeManagerImplTest, ConfigureOne) {
  AddController(BOOKMARKS);

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK);

  Configure(dtm_.get(), ModelTypeSet(BOOKMARKS));
  EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm_->state());

  FinishDownload(*dtm_, ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());

  dtm_->Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
}

// Set up a DTM with 2 controllers. configure it. One of them finishes loading
// after the  timeout. Make sure eventually all are configured.
TEST_F(SyncDataTypeManagerImplTest, ConfigureSlowLoadingType) {
  AddController(BOOKMARKS);
  AddController(APPS);

  GetController(BOOKMARKS)->SetDelayModelLoad();

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::PARTIAL_SUCCESS);

  syncer::ModelTypeSet types;
  types.Put(BOOKMARKS);
  types.Put(APPS);

  Configure(dtm_.get(), types);
  EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm_->state());

  FinishDownload(*dtm_, ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  base::OneShotTimer<ModelAssociationManager>* timer =
    dtm_->GetModelAssociationManagerForTesting()->GetTimerForTesting();

  base::Closure task = timer->user_task();
  timer->Stop();
  task.Run();

  SetConfigureDoneExpectation(DataTypeManager::OK);
  GetController(APPS)->FinishStart(DataTypeController::OK);

  SetConfigureStartExpectation();
  GetController(BOOKMARKS)->SimulateModelLoadFinishing();

  FinishDownload(*dtm_, ModelTypeSet());
  GetController(BOOKMARKS)->SimulateModelLoadFinishing();

  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());

  dtm_->Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
}


// Set up a DTM with a single controller, configure it, but stop it
// before finishing the download.  It should still be safe to run the
// download callback even after the DTM is stopped and destroyed.
TEST_F(SyncDataTypeManagerImplTest, ConfigureOneStopWhileDownloadPending) {
  AddController(BOOKMARKS);

  {
    SetConfigureStartExpectation();
    SetConfigureDoneExpectation(DataTypeManager::ABORTED);

    Configure(dtm_.get(), ModelTypeSet(BOOKMARKS));
    EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm_->state());

    dtm_->Stop();
    EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
  }

  configurer_.last_ready_task().Run(ModelTypeSet());
}

// Set up a DTM with a single controller, configure it, finish
// downloading, but stop the DTM before the controller finishes
// starting up.  It should still be safe to finish starting up the
// controller even after the DTM is stopped and destroyed.
TEST_F(SyncDataTypeManagerImplTest, ConfigureOneStopWhileStartingModel) {
  AddController(BOOKMARKS);

  {
    SetConfigureStartExpectation();
    SetConfigureDoneExpectation(DataTypeManager::ABORTED);

    Configure(dtm_.get(), ModelTypeSet(BOOKMARKS));
    EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm_->state());

    FinishDownload(*dtm_, ModelTypeSet());
    EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

    dtm_->Stop();
    EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
    dtm_.reset();
  }

  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
}

// Set up a DTM with a single controller, configure it, finish
// downloading, start the controller's model, but stop the DTM before
// the controller finishes starting up.  It should still be safe to
// finish starting up the controller even after the DTM is stopped and
// destroyed.
TEST_F(SyncDataTypeManagerImplTest, ConfigureOneStopWhileAssociating) {
  AddController(BOOKMARKS);
  {
    SetConfigureStartExpectation();
    SetConfigureDoneExpectation(DataTypeManager::ABORTED);

    Configure(dtm_.get(), ModelTypeSet(BOOKMARKS));
    EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm_->state());

    FinishDownload(*dtm_, ModelTypeSet());
    EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

    dtm_->Stop();
    EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
    dtm_.reset();
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
TEST_F(SyncDataTypeManagerImplTest, OneWaitingForCrypto) {
  AddController(PASSWORDS);

  SetConfigureStartExpectation();
  SetConfigureBlockedExpectation();

  const ModelTypeSet types(PASSWORDS);

  // Step 1.
  Configure(dtm_.get(), types);
  EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm_->state());

  // Step 2.
  FinishDownload(*dtm_, ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 3.
  GetController(PASSWORDS)->FinishStart(DataTypeController::NEEDS_CRYPTO);
  EXPECT_EQ(DataTypeManager::BLOCKED, dtm_->state());

  Mock::VerifyAndClearExpectations(&observer_);
  SetConfigureDoneExpectation(DataTypeManager::OK);

  // Step 4.
  Configure(dtm_.get(), types);
  EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm_->state());

  // Step 5.
  FinishDownload(*dtm_, ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 6.
  GetController(PASSWORDS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());

  // Step 7.
  dtm_->Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
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
TEST_F(SyncDataTypeManagerImplTest, ConfigureOneThenBoth) {
  AddController(BOOKMARKS);
  AddController(PREFERENCES);

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK);

  // Step 1.
  Configure(dtm_.get(), ModelTypeSet(BOOKMARKS));
  EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm_->state());

  // Step 2.
  FinishDownload(*dtm_, ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 3.
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());

  Mock::VerifyAndClearExpectations(&observer_);
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK);

  // Step 4.
  Configure(dtm_.get(), ModelTypeSet(BOOKMARKS, PREFERENCES));
  EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm_->state());

  // Step 5.
  FinishDownload(*dtm_, ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 6.
  GetController(PREFERENCES)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());

  // Step 7.
  dtm_->Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
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
TEST_F(SyncDataTypeManagerImplTest, ConfigureOneThenSwitch) {
  AddController(BOOKMARKS);
  AddController(PREFERENCES);

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK);

  // Step 1.
  Configure(dtm_.get(), ModelTypeSet(BOOKMARKS));
  EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm_->state());

  // Step 2.
  FinishDownload(*dtm_, ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 3.
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());

  Mock::VerifyAndClearExpectations(&observer_);
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK);

  // Step 4.
  Configure(dtm_.get(), ModelTypeSet(PREFERENCES));
  EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm_->state());

  // Step 5.
  FinishDownload(*dtm_, ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 6.
  GetController(PREFERENCES)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());

  // Step 7.
  dtm_->Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
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
TEST_F(SyncDataTypeManagerImplTest, ConfigureWhileOneInFlight) {
  AddController(BOOKMARKS);
  AddController(PREFERENCES);

  SetConfigureStartExpectation();
  SetConfigureBlockedExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK);

  // Step 1.
  Configure(dtm_.get(), ModelTypeSet(BOOKMARKS));
  EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm_->state());

  // Step 2.
  FinishDownload(*dtm_, ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 3.
  Configure(dtm_.get(), ModelTypeSet(BOOKMARKS, PREFERENCES));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 4.
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::BLOCKED, dtm_->state());

  // Pump the loop to run the posted DTMI::ConfigureImpl() task from
  // DTMI::ProcessReconfigure() (triggered by FinishStart()).
  ui_loop_.RunUntilIdle();
  EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm_->state());

  // Step 5.
  FinishDownload(*dtm_, ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 6.
  GetController(PREFERENCES)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());

  // Step 7.
  dtm_->Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
}

// Set up a DTM with one controller.  Then configure, finish
// downloading, and start the controller with an unrecoverable error.
// The unrecoverable error should cause the DTM to stop.
TEST_F(SyncDataTypeManagerImplTest, OneFailingController) {
  AddController(BOOKMARKS);

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::UNRECOVERABLE_ERROR);

  Configure(dtm_.get(), ModelTypeSet(BOOKMARKS));
  EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm_->state());

  FinishDownload(*dtm_, ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  GetController(BOOKMARKS)->FinishStart(
      DataTypeController::UNRECOVERABLE_ERROR);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
}

// Set up a DTM with two controllers.  Then:
//
//   1) Configure with both controllers.
//   2) Finish the download for step 1.
//   3) Finish starting the first controller successfully.
//   4) Finish starting the second controller with an unrecoverable error.
//
// The failure from step 4 should cause the DTM to stop.
TEST_F(SyncDataTypeManagerImplTest, SecondControllerFails) {
  AddController(BOOKMARKS);
  AddController(PREFERENCES);

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::UNRECOVERABLE_ERROR);

  // Step 1.
  Configure(dtm_.get(), ModelTypeSet(BOOKMARKS, PREFERENCES));
  EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm_->state());

  // Step 2.
  FinishDownload(*dtm_, ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 3.
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 4.
  GetController(PREFERENCES)->FinishStart(
      DataTypeController::UNRECOVERABLE_ERROR);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
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
TEST_F(SyncDataTypeManagerImplTest, OneControllerFailsAssociation) {
  AddController(BOOKMARKS);
  AddController(PREFERENCES);

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::PARTIAL_SUCCESS);

  // Step 1.
  Configure(dtm_.get(), ModelTypeSet(BOOKMARKS, PREFERENCES));
  EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm_->state());

  // Step 2.
  FinishDownload(*dtm_, ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 3.
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 4.
  GetController(PREFERENCES)->FinishStart(
      DataTypeController::ASSOCIATION_FAILED);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());

  // Step 5.
  dtm_->Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
}

// Set up a DTM with two controllers.  Then:
//
//   1) Configure with first controller.
//   2) Configure with both controllers.
//   3) Finish the download for step 1.
//   4) Finish the download for step 2.
//   5) Finish starting both controllers.
//   6) Stop the DTM.
TEST_F(SyncDataTypeManagerImplTest, ConfigureWhileDownloadPending) {
  AddController(BOOKMARKS);
  AddController(PREFERENCES);

  SetConfigureStartExpectation();
  SetConfigureBlockedExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK);

  // Step 1.
  Configure(dtm_.get(), ModelTypeSet(BOOKMARKS));
  EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm_->state());

  // Step 2.
  Configure(dtm_.get(), ModelTypeSet(BOOKMARKS, PREFERENCES));
  EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm_->state());

  // Step 3.
  FinishDownload(*dtm_, ModelTypeSet());
  EXPECT_EQ(DataTypeManager::BLOCKED, dtm_->state());

  // Pump the loop to run the posted DTMI::ConfigureImpl() task from
  // DTMI::ProcessReconfigure() (triggered by step 3).
  ui_loop_.RunUntilIdle();
  EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm_->state());

  // Step 4.
  FinishDownload(*dtm_, ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 5.
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  GetController(PREFERENCES)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());

  // Step 6.
  dtm_->Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
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
TEST_F(SyncDataTypeManagerImplTest, ConfigureWhileDownloadPendingWithFailure) {
  AddController(BOOKMARKS);
  AddController(PREFERENCES);

  SetConfigureStartExpectation();
  SetConfigureBlockedExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK);

  // Step 1.
  Configure(dtm_.get(), ModelTypeSet(BOOKMARKS));
  EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm_->state());

  // Step 2.
  Configure(dtm_.get(), ModelTypeSet(BOOKMARKS, PREFERENCES));
  EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm_->state());

  // Step 3.
  FinishDownload(*dtm_, ModelTypeSet(BOOKMARKS));
  EXPECT_EQ(DataTypeManager::BLOCKED, dtm_->state());

  // Pump the loop to run the posted DTMI::ConfigureImpl() task from
  // DTMI::ProcessReconfigure() (triggered by step 3).
  ui_loop_.RunUntilIdle();
  EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm_->state());

  // Step 4.
  FinishDownload(*dtm_, ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 5.
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  GetController(PREFERENCES)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());

  // Step 6.
  dtm_->Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
}

// Tests a Purge then Configure.  This is similar to the sequence of
// operations that would be invoked by the BackendMigrator.
TEST_F(SyncDataTypeManagerImplTest, MigrateAll) {
  AddController(BOOKMARKS);

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK);

  // Initial setup.
  Configure(dtm_.get(), ModelTypeSet(BOOKMARKS));
  FinishDownload(*dtm_, ModelTypeSet());
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);

  // We've now configured bookmarks and (implicitly) the control types.
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  Mock::VerifyAndClearExpectations(&observer_);

  // Pretend we were told to migrate all types.
  ModelTypeSet to_migrate;
  to_migrate.Put(BOOKMARKS);
  to_migrate.PutAll(syncer::ControlTypes());

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK);
  dtm_->PurgeForMigration(to_migrate,
                        syncer::CONFIGURE_REASON_MIGRATION);
  EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm_->state());

  // The DTM will call ConfigureDataTypes(), even though it is unnecessary.
  FinishDownload(*dtm_, ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  Mock::VerifyAndClearExpectations(&observer_);

  // Re-enable the migrated types.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK);
  Configure(dtm_.get(), to_migrate);
  FinishDownload(*dtm_, ModelTypeSet());
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
}

// Test receipt of a Configure request while a purge is in flight.
TEST_F(SyncDataTypeManagerImplTest, ConfigureDuringPurge) {
  AddController(BOOKMARKS);
  AddController(PREFERENCES);

  // Initial configure.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK);
  Configure(dtm_.get(), ModelTypeSet(BOOKMARKS));
  FinishDownload(*dtm_, ModelTypeSet());
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  Mock::VerifyAndClearExpectations(&observer_);

  // Purge the Nigori type.
  SetConfigureStartExpectation();
  dtm_->PurgeForMigration(ModelTypeSet(NIGORI),
                        syncer::CONFIGURE_REASON_MIGRATION);
  EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm_->state());
  Mock::VerifyAndClearExpectations(&observer_);

  // Before the backend configuration completes, ask for a different
  // set of types.  This request asks for
  // - BOOKMARKS: which is redundant because it was already enabled,
  // - PREFERENCES: which is new and will need to be downloaded, and
  // - NIGORI: (added implicitly because it is a control type) which
  //   the DTM is part-way through purging.
  SetConfigureBlockedExpectation();
  Configure(dtm_.get(), ModelTypeSet(BOOKMARKS, PREFERENCES));
  EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm_->state());

  // Invoke the callback we've been waiting for since we asked to purge NIGORI.
  FinishDownload(*dtm_, ModelTypeSet());
  EXPECT_EQ(DataTypeManager::BLOCKED, dtm_->state());
  Mock::VerifyAndClearExpectations(&observer_);

  SetConfigureDoneExpectation(DataTypeManager::OK);
  // Pump the loop to run the posted DTMI::ConfigureImpl() task from
  // DTMI::ProcessReconfigure().
  ui_loop_.RunUntilIdle();
  EXPECT_EQ(DataTypeManager::DOWNLOAD_PENDING, dtm_->state());

  // Now invoke the callback for the second configure request.
  FinishDownload(*dtm_, ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Start the preferences controller.  We don't need to start controller for
  // the NIGORI because it has none.  We don't need to start the controller for
  // the BOOKMARKS because it was never stopped.
  GetController(PREFERENCES)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
}

}  // namespace browser_sync
