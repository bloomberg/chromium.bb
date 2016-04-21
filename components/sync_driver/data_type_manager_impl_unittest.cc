// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/sync_driver/data_type_manager_impl.h"

#include "base/compiler_specific.h"
#include "base/message_loop/message_loop.h"
#include "components/sync_driver/backend_data_type_configurer.h"
#include "components/sync_driver/data_type_encryption_handler.h"
#include "components/sync_driver/data_type_manager_observer.h"
#include "components/sync_driver/data_type_status_table.h"
#include "components/sync_driver/fake_data_type_controller.h"
#include "sync/internal_api/public/activation_context.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/internal_api/public/configure_reason.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace sync_driver {

using syncer::SyncError;
using syncer::ModelType;
using syncer::ModelTypeSet;
using syncer::ModelTypeToString;
using syncer::BOOKMARKS;
using syncer::APPS;
using syncer::PASSWORDS;
using syncer::PREFERENCES;
using syncer::NIGORI;

namespace {

// Helper for unioning with control types.
ModelTypeSet AddControlTypesTo(ModelTypeSet types) {
  ModelTypeSet result = syncer::ControlTypes();
  result.PutAll(types);
  return result;
}

DataTypeStatusTable BuildStatusTable(ModelTypeSet crypto_errors,
                                     ModelTypeSet association_errors,
                                     ModelTypeSet unready_errors,
                                     ModelTypeSet unrecoverable_errors) {
  DataTypeStatusTable::TypeErrorMap error_map;
  for (ModelTypeSet::Iterator iter = crypto_errors.First(); iter.Good();
       iter.Inc()) {
    error_map[iter.Get()] = SyncError(FROM_HERE,
                                      SyncError::CRYPTO_ERROR,
                                      "crypto error expected",
                                      iter.Get());
  }
  for (ModelTypeSet::Iterator iter = association_errors.First(); iter.Good();
       iter.Inc()) {
    error_map[iter.Get()] = SyncError(FROM_HERE,
                                      SyncError::DATATYPE_ERROR,
                                      "association error expected",
                                      iter.Get());
  }
  for (ModelTypeSet::Iterator iter = unready_errors.First(); iter.Good();
       iter.Inc()) {
    error_map[iter.Get()] = SyncError(FROM_HERE,
                                      SyncError::UNREADY_ERROR,
                                      "unready error expected",
                                      iter.Get());
  }
  for (ModelTypeSet::Iterator iter = unrecoverable_errors.First(); iter.Good();
       iter.Inc()) {
    error_map[iter.Get()] = SyncError(FROM_HERE,
                                      SyncError::UNRECOVERABLE_ERROR,
                                      "unrecoverable error expected",
                                      iter.Get());
  }
  DataTypeStatusTable status_table;
  status_table.UpdateFailedDataTypes(error_map);
  return status_table;
}

// Fake BackendDataTypeConfigurer implementation that simply stores away the
// callback passed into ConfigureDataTypes.
class FakeBackendDataTypeConfigurer : public BackendDataTypeConfigurer {
 public:
  FakeBackendDataTypeConfigurer() : configure_call_count_(0) {}
  ~FakeBackendDataTypeConfigurer() override {}

  syncer::ModelTypeSet ConfigureDataTypes(
      syncer::ConfigureReason reason,
      const DataTypeConfigStateMap& config_state_map,
      const base::Callback<void(ModelTypeSet, ModelTypeSet)>& ready_task,
      const base::Callback<void()>& retry_callback) override {
    configure_call_count_++;
    last_ready_task_ = ready_task;

    for (auto iter = expected_configure_types_.begin();
         iter != expected_configure_types_.end(); ++iter) {
      if (!iter->second.Empty()) {
        EXPECT_TRUE(iter->second.Equals(
            GetDataTypesInState(iter->first, config_state_map)))
            << "State " << iter->first << " : "
            << ModelTypeSetToString(iter->second) << " v.s. "
            << ModelTypeSetToString(
                   GetDataTypesInState(iter->first, config_state_map));
      }
    }
    return ready_types_;
  }

  void ActivateDirectoryDataType(syncer::ModelType type,
                                 syncer::ModelSafeGroup group,
                                 ChangeProcessor* change_processor) override {
    activated_types_.Put(type);
  }
  void DeactivateDirectoryDataType(syncer::ModelType type) override {
    activated_types_.Remove(type);
  }

  void ActivateNonBlockingDataType(syncer::ModelType type,
                                   std::unique_ptr<syncer_v2::ActivationContext>
                                       activation_context) override {
    // TODO(stanisc): crbug.com/515962: Add test coverage.
  }

  void DeactivateNonBlockingDataType(syncer::ModelType type) override {
    // TODO(stanisc): crbug.com/515962: Add test coverage.
  }

  base::Callback<void(ModelTypeSet, ModelTypeSet)> last_ready_task() const {
    return last_ready_task_;
  }

  void set_expected_configure_types(DataTypeConfigState config_state,
                                    ModelTypeSet types) {
    expected_configure_types_[config_state] = types;
  }

  void set_ready_types(ModelTypeSet types) {
    ready_types_ = types;
  }

  const ModelTypeSet activated_types() { return activated_types_; }

  int configure_call_count() const { return configure_call_count_; }

 private:
  base::Callback<void(ModelTypeSet, ModelTypeSet)> last_ready_task_;
  std::map<DataTypeConfigState, ModelTypeSet> expected_configure_types_;
  ModelTypeSet activated_types_;
  ModelTypeSet ready_types_;
  int configure_call_count_;
};

// DataTypeManagerObserver implementation.
class FakeDataTypeManagerObserver : public DataTypeManagerObserver {
 public:
  FakeDataTypeManagerObserver() { ResetExpectations(); }
  ~FakeDataTypeManagerObserver() override {
    EXPECT_FALSE(start_expected_);
    DataTypeManager::ConfigureResult default_result;
    EXPECT_EQ(done_expectation_.status, default_result.status);
    EXPECT_TRUE(
        done_expectation_.data_type_status_table.GetFailedTypes().Empty());
  }

  void ExpectStart() {
    start_expected_ = true;
  }
  void ExpectDone(const DataTypeManager::ConfigureResult& result) {
    done_expectation_ = result;
  }
  void ResetExpectations() {
    start_expected_ = false;
    done_expectation_ = DataTypeManager::ConfigureResult();
  }

  void OnConfigureDone(
      const DataTypeManager::ConfigureResult& result) override {
    EXPECT_EQ(done_expectation_.status, result.status);
    DataTypeStatusTable::TypeErrorMap errors =
        result.data_type_status_table.GetAllErrors();
    DataTypeStatusTable::TypeErrorMap expected_errors =
        done_expectation_.data_type_status_table.GetAllErrors();
    ASSERT_EQ(expected_errors.size(), errors.size());
    for (DataTypeStatusTable::TypeErrorMap::const_iterator iter =
             expected_errors.begin();
         iter != expected_errors.end();
         ++iter) {
      ASSERT_TRUE(errors.find(iter->first) != errors.end());
      ASSERT_EQ(iter->second.error_type(),
                errors.find(iter->first)->second.error_type());
    }
    done_expectation_ = DataTypeManager::ConfigureResult();
  }

  void OnConfigureStart() override {
    EXPECT_TRUE(start_expected_);
    start_expected_ = false;
  }

 private:
  bool start_expected_ = true;
  DataTypeManager::ConfigureResult done_expectation_;
};

class FakeDataTypeEncryptionHandler : public DataTypeEncryptionHandler {
 public:
  FakeDataTypeEncryptionHandler();
  ~FakeDataTypeEncryptionHandler() override;

  bool IsPassphraseRequired() const override;
  ModelTypeSet GetEncryptedDataTypes() const override;

  void set_passphrase_required(bool passphrase_required) {
    passphrase_required_ = passphrase_required;
  }
  void set_encrypted_types(ModelTypeSet encrypted_types) {
    encrypted_types_ = encrypted_types;
  }
 private:
  bool passphrase_required_;
  ModelTypeSet encrypted_types_;
};

FakeDataTypeEncryptionHandler::FakeDataTypeEncryptionHandler()
    : passphrase_required_(false) {}
FakeDataTypeEncryptionHandler::~FakeDataTypeEncryptionHandler() {}

bool FakeDataTypeEncryptionHandler::IsPassphraseRequired() const {
  return passphrase_required_;
}

ModelTypeSet
FakeDataTypeEncryptionHandler::GetEncryptedDataTypes() const {
  return encrypted_types_;
}

}  // namespace

class TestDataTypeManager : public DataTypeManagerImpl {
 public:
  TestDataTypeManager(
      const syncer::WeakHandle<syncer::DataTypeDebugInfoListener>&
          debug_info_listener,
      BackendDataTypeConfigurer* configurer,
      const DataTypeController::TypeMap* controllers,
      const DataTypeEncryptionHandler* encryption_handler,
      DataTypeManagerObserver* observer)
      : DataTypeManagerImpl(debug_info_listener,
                            controllers,
                            encryption_handler,
                            configurer,
                            observer),
        custom_priority_types_(syncer::ControlTypes()) {}

  void set_priority_types(const ModelTypeSet& priority_types) {
    custom_priority_types_ = priority_types;
  }

  DataTypeManager::ConfigureResult configure_result() const {
    return configure_result_;
  }

  void OnModelAssociationDone(
      const DataTypeManager::ConfigureResult& result) override {
    configure_result_ = result;
    DataTypeManagerImpl::OnModelAssociationDone(result);
  }

 private:
  ModelTypeSet GetPriorityTypes() const override {
    return custom_priority_types_;
  }

  ModelTypeSet custom_priority_types_;
  DataTypeManager::ConfigureResult configure_result_;
};

// The actual test harness class, parametrized on nigori state (i.e., tests are
// run both configuring with nigori, and configuring without).
class SyncDataTypeManagerImplTest : public testing::Test {
 public:
  SyncDataTypeManagerImplTest() {}

  ~SyncDataTypeManagerImplTest() override {}

 protected:
  void SetUp() override {
    dtm_.reset(
        new TestDataTypeManager(
            syncer::WeakHandle<syncer::DataTypeDebugInfoListener>(),
            &configurer_,
            &controllers_,
            &encryption_handler_,
            &observer_));
  }

  void SetConfigureStartExpectation() {
    observer_.ExpectStart();
  }

  void SetConfigureDoneExpectation(DataTypeManager::ConfigureStatus status,
                                   const DataTypeStatusTable& status_table) {
    DataTypeManager::ConfigureResult result;
    result.status = status;
    result.data_type_status_table = status_table;
    observer_.ExpectDone(result);
  }

  // Configure the given DTM with the given desired types.
  void Configure(DataTypeManagerImpl* dtm,
                 const ModelTypeSet& desired_types) {
    dtm->Configure(desired_types, syncer::CONFIGURE_REASON_RECONFIGURATION);
  }

  // Finish downloading for the given DTM. Should be done only after
  // a call to Configure().
  void FinishDownload(const DataTypeManager& dtm,
                      ModelTypeSet types_to_configure,
                      ModelTypeSet failed_download_types) {
    EXPECT_EQ(DataTypeManager::CONFIGURING, dtm.state());
    ASSERT_FALSE(configurer_.last_ready_task().is_null());
    configurer_.last_ready_task().Run(
        syncer::Difference(types_to_configure, failed_download_types),
        failed_download_types);
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

  void FailEncryptionFor(ModelTypeSet encrypted_types) {
    encryption_handler_.set_passphrase_required(true);
    encryption_handler_.set_encrypted_types(encrypted_types);
  }

  base::MessageLoopForUI ui_loop_;
  DataTypeController::TypeMap controllers_;
  FakeBackendDataTypeConfigurer configurer_;
  FakeDataTypeManagerObserver observer_;
  std::unique_ptr<TestDataTypeManager> dtm_;
  FakeDataTypeEncryptionHandler encryption_handler_;
};

// Set up a DTM with no controllers, configure it, finish downloading,
// and then stop it.
TEST_F(SyncDataTypeManagerImplTest, NoControllers) {
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  Configure(dtm_.get(), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  FinishDownload(*dtm_, ModelTypeSet(), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());

  dtm_->Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
}

// Set up a DTM with a single controller, configure it, finish
// downloading, finish starting the controller, and then stop the DTM.
TEST_F(SyncDataTypeManagerImplTest, ConfigureOne) {
  AddController(BOOKMARKS);

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  Configure(dtm_.get(), ModelTypeSet(BOOKMARKS));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  FinishDownload(*dtm_, ModelTypeSet(), ModelTypeSet());
  FinishDownload(*dtm_, ModelTypeSet(BOOKMARKS), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(1U, configurer_.activated_types().Size());

  dtm_->Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
  EXPECT_TRUE(configurer_.activated_types().Empty());
}

// Set up a DTM with a single controller, configure it, but stop it
// before finishing the download.  It should still be safe to run the
// download callback even after the DTM is stopped and destroyed.
TEST_F(SyncDataTypeManagerImplTest, ConfigureOneStopWhileDownloadPending) {
  AddController(BOOKMARKS);

  {
    SetConfigureStartExpectation();
    SetConfigureDoneExpectation(DataTypeManager::ABORTED,
                                DataTypeStatusTable());

    Configure(dtm_.get(), ModelTypeSet(BOOKMARKS));
    EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

    dtm_->Stop();
    EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
  }

  configurer_.last_ready_task().Run(ModelTypeSet(BOOKMARKS), ModelTypeSet());
  EXPECT_TRUE(configurer_.activated_types().Empty());
}

// Set up a DTM with a single controller, configure it, finish
// downloading, but stop the DTM before the controller finishes
// starting up.  It should still be safe to finish starting up the
// controller even after the DTM is stopped and destroyed.
TEST_F(SyncDataTypeManagerImplTest, ConfigureOneStopWhileStartingModel) {
  AddController(BOOKMARKS);

  {
    SetConfigureStartExpectation();
    SetConfigureDoneExpectation(DataTypeManager::ABORTED,
                                DataTypeStatusTable());

    Configure(dtm_.get(), ModelTypeSet(BOOKMARKS));
    EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

    FinishDownload(*dtm_, ModelTypeSet(), ModelTypeSet());
    FinishDownload(*dtm_, ModelTypeSet(BOOKMARKS), ModelTypeSet());
    EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

    dtm_->Stop();
    EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
    dtm_.reset();
  }

  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_TRUE(configurer_.activated_types().Empty());
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
    SetConfigureDoneExpectation(DataTypeManager::ABORTED,
                                DataTypeStatusTable());

    Configure(dtm_.get(), ModelTypeSet(BOOKMARKS));
    EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

    FinishDownload(*dtm_, ModelTypeSet(), ModelTypeSet());
    FinishDownload(*dtm_, ModelTypeSet(BOOKMARKS), ModelTypeSet());
    EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
    EXPECT_TRUE(configurer_.activated_types().Empty());

    dtm_->Stop();
    EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
    dtm_.reset();
  }

  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_TRUE(configurer_.activated_types().Empty());
}

// Set up a DTM with a single controller.  Then:
//
//   1) Configure.
//   2) Finish the download for step 1.
//   3) Finish starting the controller with the NEEDS_CRYPTO status.
//   4) Complete download for the reconfiguration without the controller.
//   5) Stop the DTM.
TEST_F(SyncDataTypeManagerImplTest, OneWaitingForCrypto) {
  AddController(PASSWORDS);

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK,
                              BuildStatusTable(ModelTypeSet(PASSWORDS),
                                               ModelTypeSet(),
                                               ModelTypeSet(),
                                               ModelTypeSet()));

  const ModelTypeSet types(PASSWORDS);
  dtm_->set_priority_types(AddControlTypesTo(types));

  // Step 1.
  Configure(dtm_.get(), types);
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 2.
  FinishDownload(*dtm_, ModelTypeSet(), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 3.
  FailEncryptionFor(types);
  GetController(PASSWORDS)->FinishStart(DataTypeController::NEEDS_CRYPTO);
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 4.
  FinishDownload(*dtm_, ModelTypeSet(), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());

  // Step 5.
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
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  // Step 1.
  Configure(dtm_.get(), ModelTypeSet(BOOKMARKS));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 2.
  FinishDownload(*dtm_, ModelTypeSet(), ModelTypeSet());
  FinishDownload(*dtm_, ModelTypeSet(BOOKMARKS), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 3.
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());

  observer_.ResetExpectations();
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  // Step 4.
  Configure(dtm_.get(), ModelTypeSet(BOOKMARKS, PREFERENCES));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 5.
  FinishDownload(*dtm_, ModelTypeSet(), ModelTypeSet());
  FinishDownload(*dtm_, ModelTypeSet(BOOKMARKS, PREFERENCES), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 6.
  GetController(PREFERENCES)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(2U, configurer_.activated_types().Size());

  // Step 7.
  dtm_->Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
  EXPECT_TRUE(configurer_.activated_types().Empty());
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
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  // Step 1.
  Configure(dtm_.get(), ModelTypeSet(BOOKMARKS));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 2.
  FinishDownload(*dtm_, ModelTypeSet(), ModelTypeSet());
  FinishDownload(*dtm_, ModelTypeSet(BOOKMARKS), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 3.
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());

  observer_.ResetExpectations();
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  // Step 4.
  Configure(dtm_.get(), ModelTypeSet(PREFERENCES));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 5.
  FinishDownload(*dtm_, ModelTypeSet(), ModelTypeSet());
  FinishDownload(*dtm_, ModelTypeSet(PREFERENCES), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 6.
  GetController(PREFERENCES)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(1U, configurer_.activated_types().Size());

  // Step 7.
  dtm_->Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
  EXPECT_TRUE(configurer_.activated_types().Empty());
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
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  // Step 1.
  Configure(dtm_.get(), ModelTypeSet(BOOKMARKS));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 2.
  FinishDownload(*dtm_, ModelTypeSet(), ModelTypeSet());
  FinishDownload(*dtm_, ModelTypeSet(BOOKMARKS), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 3.
  Configure(dtm_.get(), ModelTypeSet(BOOKMARKS, PREFERENCES));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 4.
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 5.
  FinishDownload(*dtm_, ModelTypeSet(), ModelTypeSet());
  FinishDownload(*dtm_, ModelTypeSet(BOOKMARKS, PREFERENCES), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 6.
  GetController(PREFERENCES)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(2U, configurer_.activated_types().Size());

  // Step 7.
  dtm_->Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
  EXPECT_TRUE(configurer_.activated_types().Empty());
}

// Set up a DTM with one controller.  Then configure, finish
// downloading, and start the controller with an unrecoverable error.
// The unrecoverable error should cause the DTM to stop.
TEST_F(SyncDataTypeManagerImplTest, OneFailingController) {
  AddController(BOOKMARKS);

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::UNRECOVERABLE_ERROR,
                              BuildStatusTable(ModelTypeSet(),
                                               ModelTypeSet(),
                                               ModelTypeSet(),
                                               ModelTypeSet(BOOKMARKS)));

  Configure(dtm_.get(), ModelTypeSet(BOOKMARKS));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  FinishDownload(*dtm_, ModelTypeSet(), ModelTypeSet());
  FinishDownload(*dtm_, ModelTypeSet(BOOKMARKS), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_TRUE(configurer_.activated_types().Empty());

  GetController(BOOKMARKS)->FinishStart(
      DataTypeController::UNRECOVERABLE_ERROR);
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
  EXPECT_TRUE(configurer_.activated_types().Empty());
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
  SetConfigureDoneExpectation(DataTypeManager::UNRECOVERABLE_ERROR,
                              BuildStatusTable(ModelTypeSet(),
                                               ModelTypeSet(),
                                               ModelTypeSet(),
                                               ModelTypeSet(PREFERENCES)));

  // Step 1.
  Configure(dtm_.get(), ModelTypeSet(BOOKMARKS, PREFERENCES));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 2.
  FinishDownload(*dtm_, ModelTypeSet(), ModelTypeSet());
  FinishDownload(*dtm_, ModelTypeSet(BOOKMARKS, PREFERENCES), ModelTypeSet());
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
//   3) Finish starting the first controller successfully.
//   4) Finish starting the second controller with an association failure.
//   5) Finish the purge/reconfigure without the failed type.
//   6) Stop the DTM.
//
// The association failure from step 3 should be ignored.
//
// TODO(akalin): Check that the data type that failed association is
// recorded in the CONFIGURE_DONE notification.
TEST_F(SyncDataTypeManagerImplTest, OneControllerFailsAssociation) {
  AddController(BOOKMARKS);
  AddController(PREFERENCES);

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK,
                              BuildStatusTable(ModelTypeSet(),
                                               ModelTypeSet(PREFERENCES),
                                               ModelTypeSet(),
                                               ModelTypeSet()));

  // Step 1.
  Configure(dtm_.get(), ModelTypeSet(BOOKMARKS, PREFERENCES));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 2.
  FinishDownload(*dtm_, ModelTypeSet(), ModelTypeSet());
  FinishDownload(*dtm_, ModelTypeSet(BOOKMARKS, PREFERENCES), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 3.
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 4.
  GetController(PREFERENCES)->FinishStart(
      DataTypeController::ASSOCIATION_FAILED);
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 5.
  FinishDownload(*dtm_, ModelTypeSet(), ModelTypeSet());
  FinishDownload(*dtm_, ModelTypeSet(BOOKMARKS), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(1U, configurer_.activated_types().Size());

  // Step 6.
  dtm_->Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
  EXPECT_TRUE(configurer_.activated_types().Empty());
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
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  // Step 1.
  Configure(dtm_.get(), ModelTypeSet(BOOKMARKS));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 2.
  Configure(dtm_.get(), ModelTypeSet(BOOKMARKS, PREFERENCES));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 3.
  FinishDownload(*dtm_, ModelTypeSet(), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 4.
  FinishDownload(*dtm_, ModelTypeSet(), ModelTypeSet());
  FinishDownload(*dtm_, ModelTypeSet(BOOKMARKS, PREFERENCES), ModelTypeSet());
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
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  // Step 1.
  Configure(dtm_.get(), ModelTypeSet(BOOKMARKS));
  FinishDownload(*dtm_, ModelTypeSet(), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 2.
  Configure(dtm_.get(), ModelTypeSet(BOOKMARKS, PREFERENCES));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 3.
  FinishDownload(*dtm_, ModelTypeSet(), ModelTypeSet());
  FinishDownload(*dtm_, ModelTypeSet(BOOKMARKS), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Step 4.
  FinishDownload(*dtm_, ModelTypeSet(BOOKMARKS, PREFERENCES), ModelTypeSet());
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
  dtm_->set_priority_types(AddControlTypesTo(ModelTypeSet(BOOKMARKS)));

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  // Initial setup.
  Configure(dtm_.get(), ModelTypeSet(BOOKMARKS));
  FinishDownload(*dtm_, ModelTypeSet(BOOKMARKS), ModelTypeSet());
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);

  // We've now configured bookmarks and (implicitly) the control types.
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  observer_.ResetExpectations();

  // Pretend we were told to migrate all types.
  ModelTypeSet to_migrate;
  to_migrate.Put(BOOKMARKS);
  to_migrate.PutAll(syncer::ControlTypes());

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());
  dtm_->PurgeForMigration(to_migrate,
                          syncer::CONFIGURE_REASON_MIGRATION);
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // The DTM will call ConfigureDataTypes(), even though it is unnecessary.
  FinishDownload(*dtm_, ModelTypeSet(), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  observer_.ResetExpectations();

  // Re-enable the migrated types.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());
  Configure(dtm_.get(), to_migrate);
  FinishDownload(*dtm_, to_migrate, ModelTypeSet());
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
}

// Test receipt of a Configure request while a purge is in flight.
TEST_F(SyncDataTypeManagerImplTest, ConfigureDuringPurge) {
  AddController(BOOKMARKS);
  AddController(PREFERENCES);

  // Initial configure.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());
  Configure(dtm_.get(), ModelTypeSet(BOOKMARKS));
  FinishDownload(*dtm_, ModelTypeSet(), ModelTypeSet());
  FinishDownload(*dtm_, ModelTypeSet(BOOKMARKS), ModelTypeSet());
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  observer_.ResetExpectations();

  // Purge the Nigori type.
  SetConfigureStartExpectation();
  dtm_->PurgeForMigration(ModelTypeSet(NIGORI),
                          syncer::CONFIGURE_REASON_MIGRATION);
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  observer_.ResetExpectations();

  // Before the backend configuration completes, ask for a different
  // set of types.  This request asks for
  // - BOOKMARKS: which is redundant because it was already enabled,
  // - PREFERENCES: which is new and will need to be downloaded, and
  // - NIGORI: (added implicitly because it is a control type) which
  //   the DTM is part-way through purging.
  Configure(dtm_.get(), ModelTypeSet(BOOKMARKS, PREFERENCES));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Invoke the callback we've been waiting for since we asked to purge NIGORI.
  FinishDownload(*dtm_, ModelTypeSet(), ModelTypeSet());
  observer_.ResetExpectations();

  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Now invoke the callback for the second configure request.
  FinishDownload(*dtm_, ModelTypeSet(), ModelTypeSet());
  FinishDownload(*dtm_, ModelTypeSet(BOOKMARKS, PREFERENCES), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Start the preferences controller.  We don't need to start controller for
  // the NIGORI because it has none.  We don't need to start the controller for
  // the BOOKMARKS because it was never stopped.
  GetController(PREFERENCES)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
}

TEST_F(SyncDataTypeManagerImplTest, PrioritizedConfiguration) {
  AddController(BOOKMARKS);
  AddController(PREFERENCES);

  dtm_->set_priority_types(
      AddControlTypesTo(ModelTypeSet(PREFERENCES)));

  // Initial configure.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  // Initially only PREFERENCES is configured.
  configurer_.set_expected_configure_types(
      BackendDataTypeConfigurer::CONFIGURE_ACTIVE,
      AddControlTypesTo(ModelTypeSet(PREFERENCES)));
  Configure(dtm_.get(), ModelTypeSet(BOOKMARKS, PREFERENCES));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // BOOKMARKS is configured after download of PREFERENCES finishes.
  configurer_.set_expected_configure_types(
      BackendDataTypeConfigurer::CONFIGURE_ACTIVE, ModelTypeSet(BOOKMARKS));
  FinishDownload(*dtm_, ModelTypeSet(PREFERENCES), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  FinishDownload(*dtm_, ModelTypeSet(BOOKMARKS), ModelTypeSet());
  GetController(PREFERENCES)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
}

TEST_F(SyncDataTypeManagerImplTest, PrioritizedConfigurationReconfigure) {
  AddController(BOOKMARKS);
  AddController(PREFERENCES);
  AddController(APPS);

  dtm_->set_priority_types(
      AddControlTypesTo(ModelTypeSet(PREFERENCES)));

  // Initial configure.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  // Reconfigure while associating PREFERENCES and downloading BOOKMARKS.
  configurer_.set_expected_configure_types(
      BackendDataTypeConfigurer::CONFIGURE_ACTIVE,
      AddControlTypesTo(ModelTypeSet(PREFERENCES)));
  Configure(dtm_.get(), ModelTypeSet(BOOKMARKS, PREFERENCES));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  configurer_.set_expected_configure_types(
      BackendDataTypeConfigurer::CONFIGURE_ACTIVE, ModelTypeSet(BOOKMARKS));
  FinishDownload(*dtm_, ModelTypeSet(PREFERENCES), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Enable syncing for APPS.
  Configure(dtm_.get(), ModelTypeSet(BOOKMARKS, PREFERENCES, APPS));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Reconfiguration starts after downloading and association of previous
  // types finish.
  configurer_.set_expected_configure_types(
      BackendDataTypeConfigurer::CONFIGURE_ACTIVE,
      AddControlTypesTo(ModelTypeSet(PREFERENCES)));
  FinishDownload(*dtm_, ModelTypeSet(BOOKMARKS), ModelTypeSet());
  GetController(PREFERENCES)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  configurer_.set_expected_configure_types(
      BackendDataTypeConfigurer::CONFIGURE_ACTIVE,
      ModelTypeSet(BOOKMARKS, APPS));
  FinishDownload(*dtm_, ModelTypeSet(PREFERENCES), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  FinishDownload(*dtm_, ModelTypeSet(BOOKMARKS, APPS), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Skip calling FinishStart() for PREFENCES because it's already started in
  // first configuration.
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  GetController(APPS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
}

TEST_F(SyncDataTypeManagerImplTest, PrioritizedConfigurationStop) {
  AddController(BOOKMARKS);
  AddController(PREFERENCES);

  dtm_->set_priority_types(
      AddControlTypesTo(ModelTypeSet(PREFERENCES)));

  // Initial configure.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::ABORTED, DataTypeStatusTable());

  // Initially only PREFERENCES is configured.
  configurer_.set_expected_configure_types(
      BackendDataTypeConfigurer::CONFIGURE_ACTIVE,
      AddControlTypesTo(ModelTypeSet(PREFERENCES)));
  Configure(dtm_.get(), ModelTypeSet(BOOKMARKS, PREFERENCES));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // BOOKMARKS is configured after download of PREFERENCES finishes.
  configurer_.set_expected_configure_types(
      BackendDataTypeConfigurer::CONFIGURE_ACTIVE, ModelTypeSet(BOOKMARKS));
  FinishDownload(*dtm_, ModelTypeSet(PREFERENCES), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // PREFERENCES controller is associating while BOOKMARKS is downloading.
  EXPECT_EQ(DataTypeController::ASSOCIATING,
            GetController(PREFERENCES)->state());
  EXPECT_EQ(DataTypeController::MODEL_LOADED,
            GetController(BOOKMARKS)->state());

  dtm_->Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
  EXPECT_EQ(DataTypeController::NOT_RUNNING,
            GetController(PREFERENCES)->state());
  EXPECT_EQ(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());
}

TEST_F(SyncDataTypeManagerImplTest, PrioritizedConfigurationDownloadError) {
  AddController(BOOKMARKS);
  AddController(PREFERENCES);

  dtm_->set_priority_types(
      AddControlTypesTo(ModelTypeSet(PREFERENCES)));

  // Initial configure. Bookmarks will fail to associate due to the download
  // failure.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK,
                              BuildStatusTable(ModelTypeSet(),
                                               ModelTypeSet(BOOKMARKS),
                                               ModelTypeSet(),
                                               ModelTypeSet()));

  // Initially only PREFERENCES is configured.
  configurer_.set_expected_configure_types(
      BackendDataTypeConfigurer::CONFIGURE_ACTIVE,
      AddControlTypesTo(ModelTypeSet(PREFERENCES)));
  Configure(dtm_.get(), ModelTypeSet(BOOKMARKS, PREFERENCES));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // BOOKMARKS is configured after download of PREFERENCES finishes.
  configurer_.set_expected_configure_types(
      BackendDataTypeConfigurer::CONFIGURE_ACTIVE, ModelTypeSet(BOOKMARKS));
  FinishDownload(*dtm_, ModelTypeSet(PREFERENCES), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // PREFERENCES controller is associating while BOOKMARKS is downloading.
  EXPECT_EQ(DataTypeController::ASSOCIATING,
            GetController(PREFERENCES)->state());
  EXPECT_EQ(DataTypeController::MODEL_LOADED,
            GetController(BOOKMARKS)->state());

  // Make BOOKMARKS download fail. Preferences is still associating.
  FinishDownload(*dtm_, ModelTypeSet(), ModelTypeSet(BOOKMARKS));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(DataTypeController::ASSOCIATING,
            GetController(PREFERENCES)->state());

  // Finish association of PREFERENCES. This will trigger a reconfiguration to
  // disable bookmarks.
  configurer_.set_expected_configure_types(
      BackendDataTypeConfigurer::CONFIGURE_ACTIVE,
      AddControlTypesTo(ModelTypeSet(PREFERENCES)));
  GetController(PREFERENCES)->FinishStart(DataTypeController::OK);
  FinishDownload(*dtm_, ModelTypeSet(PREFERENCES), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(DataTypeController::RUNNING,
            GetController(PREFERENCES)->state());
  EXPECT_EQ(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());
}

TEST_F(SyncDataTypeManagerImplTest, HighPriorityAssociationFailure) {
  AddController(PREFERENCES);   // Will fail.
  AddController(BOOKMARKS);     // Will succeed.

  dtm_->set_priority_types(
      AddControlTypesTo(ModelTypeSet(PREFERENCES)));

  // Initial configure.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK,
                              BuildStatusTable(ModelTypeSet(),
                                               ModelTypeSet(PREFERENCES),
                                               ModelTypeSet(),
                                               ModelTypeSet()));

  // Initially only PREFERENCES is configured.
  configurer_.set_expected_configure_types(
      BackendDataTypeConfigurer::CONFIGURE_ACTIVE,
      AddControlTypesTo(ModelTypeSet(PREFERENCES)));
  Configure(dtm_.get(), ModelTypeSet(BOOKMARKS, PREFERENCES));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // BOOKMARKS is configured after download of PREFERENCES finishes.
  configurer_.set_expected_configure_types(
      BackendDataTypeConfigurer::CONFIGURE_ACTIVE, ModelTypeSet(BOOKMARKS));
  FinishDownload(*dtm_, ModelTypeSet(PREFERENCES), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // PREFERENCES controller is associating while BOOKMARKS is downloading.
  EXPECT_EQ(DataTypeController::ASSOCIATING,
            GetController(PREFERENCES)->state());
  EXPECT_EQ(DataTypeController::MODEL_LOADED,
            GetController(BOOKMARKS)->state());

  // Make PREFERENCES association fail.
  GetController(PREFERENCES)->FinishStart(
      DataTypeController::ASSOCIATION_FAILED);
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Reconfigure without PREFERENCES after the BOOKMARKS download completes,
  // then reconfigure with BOOKMARKS.
  configurer_.set_expected_configure_types(
      BackendDataTypeConfigurer::CONFIGURE_ACTIVE, syncer::ControlTypes());
  FinishDownload(*dtm_, ModelTypeSet(BOOKMARKS), ModelTypeSet());
  configurer_.set_expected_configure_types(
      BackendDataTypeConfigurer::CONFIGURE_ACTIVE, ModelTypeSet(BOOKMARKS));
  FinishDownload(*dtm_, ModelTypeSet(), ModelTypeSet());

  // Reconfigure with BOOKMARKS.
  FinishDownload(*dtm_, ModelTypeSet(BOOKMARKS), ModelTypeSet());
  EXPECT_EQ(DataTypeController::ASSOCIATING,
            GetController(BOOKMARKS)->state());
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);

  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(DataTypeController::NOT_RUNNING,
            GetController(PREFERENCES)->state());
  EXPECT_EQ(DataTypeController::RUNNING, GetController(BOOKMARKS)->state());
}

TEST_F(SyncDataTypeManagerImplTest, LowPriorityAssociationFailure) {
  AddController(PREFERENCES);  // Will succeed.
  AddController(BOOKMARKS);    // Will fail.

  dtm_->set_priority_types(
      AddControlTypesTo(ModelTypeSet(PREFERENCES)));

  // Initial configure.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK,
                              BuildStatusTable(ModelTypeSet(),
                                               ModelTypeSet(BOOKMARKS),
                                               ModelTypeSet(),
                                               ModelTypeSet()));

  // Initially only PREFERENCES is configured.
  configurer_.set_expected_configure_types(
      BackendDataTypeConfigurer::CONFIGURE_ACTIVE,
      AddControlTypesTo(ModelTypeSet(PREFERENCES)));
  Configure(dtm_.get(), ModelTypeSet(BOOKMARKS, PREFERENCES));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // BOOKMARKS is configured after download of PREFERENCES finishes.
  configurer_.set_expected_configure_types(
      BackendDataTypeConfigurer::CONFIGURE_ACTIVE, ModelTypeSet(BOOKMARKS));
  FinishDownload(*dtm_, ModelTypeSet(PREFERENCES), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // PREFERENCES controller is associating while BOOKMARKS is downloading.
  EXPECT_EQ(DataTypeController::ASSOCIATING,
            GetController(PREFERENCES)->state());
  EXPECT_EQ(DataTypeController::MODEL_LOADED,
            GetController(BOOKMARKS)->state());

  // BOOKMARKS finishes downloading and PREFERENCES finishes associating.
  FinishDownload(*dtm_, ModelTypeSet(BOOKMARKS), ModelTypeSet());
  GetController(PREFERENCES)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeController::RUNNING, GetController(PREFERENCES)->state());

  // Make BOOKMARKS association fail, which triggers reconfigure with only
  // PREFERENCES.
  configurer_.set_expected_configure_types(
      BackendDataTypeConfigurer::CONFIGURE_ACTIVE,
      AddControlTypesTo(ModelTypeSet(PREFERENCES)));
  GetController(BOOKMARKS)->FinishStart(DataTypeController::ASSOCIATION_FAILED);
  EXPECT_EQ(DataTypeController::NOT_RUNNING,
            GetController(BOOKMARKS)->state());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Finish configuration with only PREFERENCES.
  configurer_.set_expected_configure_types(
      BackendDataTypeConfigurer::CONFIGURE_ACTIVE, ModelTypeSet());
  FinishDownload(*dtm_, ModelTypeSet(PREFERENCES), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(DataTypeController::RUNNING, GetController(PREFERENCES)->state());
  EXPECT_EQ(DataTypeController::NOT_RUNNING,
            GetController(BOOKMARKS)->state());
}

TEST_F(SyncDataTypeManagerImplTest, FilterDesiredTypes) {
  AddController(BOOKMARKS);

  ModelTypeSet types(BOOKMARKS, APPS);
  dtm_->set_priority_types(AddControlTypesTo(types));

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  ModelTypeSet expected_types = syncer::ControlTypes();
  expected_types.Put(BOOKMARKS);
  // APPS is filtered out because there's no controller for it.
  configurer_.set_expected_configure_types(
      BackendDataTypeConfigurer::CONFIGURE_ACTIVE, expected_types);
  Configure(dtm_.get(), types);
  FinishDownload(*dtm_, ModelTypeSet(BOOKMARKS), ModelTypeSet());
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);

  dtm_->Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
}

TEST_F(SyncDataTypeManagerImplTest, ReenableAfterDataTypeError) {
  AddController(PREFERENCES);  // Will succeed.
  AddController(BOOKMARKS);    // Will be disabled due to datatype error.

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK,
                              BuildStatusTable(ModelTypeSet(),
                                               ModelTypeSet(BOOKMARKS),
                                               ModelTypeSet(),
                                               ModelTypeSet()));

  Configure(dtm_.get(), ModelTypeSet(BOOKMARKS, PREFERENCES));
  FinishDownload(*dtm_, ModelTypeSet(), ModelTypeSet());
  FinishDownload(*dtm_, ModelTypeSet(PREFERENCES, BOOKMARKS), ModelTypeSet());
  GetController(PREFERENCES)->FinishStart(DataTypeController::OK);
  GetController(BOOKMARKS)
      ->FinishStart(DataTypeController::ASSOCIATION_FAILED);
  FinishDownload(*dtm_, ModelTypeSet(), ModelTypeSet());  // Reconfig for error.
  FinishDownload(*dtm_, ModelTypeSet(), ModelTypeSet());  // Reconfig for error.
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(DataTypeController::RUNNING, GetController(PREFERENCES)->state());
  EXPECT_EQ(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());

  observer_.ResetExpectations();

  // Re-enable bookmarks.
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());
  dtm_->ReenableType(syncer::BOOKMARKS);

  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  FinishDownload(*dtm_, ModelTypeSet(), ModelTypeSet());
  FinishDownload(*dtm_, ModelTypeSet(BOOKMARKS), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(DataTypeController::RUNNING, GetController(PREFERENCES)->state());
  EXPECT_EQ(DataTypeController::RUNNING, GetController(BOOKMARKS)->state());

  // Should do nothing.
  dtm_->ReenableType(syncer::BOOKMARKS);
}

TEST_F(SyncDataTypeManagerImplTest, UnreadyType) {
  AddController(BOOKMARKS);
  GetController(BOOKMARKS)->SetReadyForStart(false);

  // Bookmarks is never started due to being unready.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK,
                              BuildStatusTable(ModelTypeSet(),
                                               ModelTypeSet(),
                                               ModelTypeSet(BOOKMARKS),
                                               ModelTypeSet()));
  Configure(dtm_.get(), ModelTypeSet(BOOKMARKS));
  FinishDownload(*dtm_, ModelTypeSet(), ModelTypeSet());
  EXPECT_EQ(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(0U, configurer_.activated_types().Size());
  observer_.ResetExpectations();

  // Bookmarks should start normally now.
  GetController(BOOKMARKS)->SetReadyForStart(true);
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());
  dtm_->ReenableType(BOOKMARKS);
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  FinishDownload(*dtm_, ModelTypeSet(), ModelTypeSet());
  FinishDownload(*dtm_, ModelTypeSet(BOOKMARKS), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(1U, configurer_.activated_types().Size());

  // Should do nothing.
  observer_.ResetExpectations();
  dtm_->ReenableType(BOOKMARKS);

  dtm_->Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
  EXPECT_TRUE(configurer_.activated_types().Empty());
}

TEST_F(SyncDataTypeManagerImplTest, ModelLoadError) {
  AddController(BOOKMARKS);
  GetController(BOOKMARKS)->SetModelLoadError(syncer::SyncError(
        FROM_HERE, SyncError::DATATYPE_ERROR, "load error", BOOKMARKS));

  // Bookmarks is never started due to hitting a model load error.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK,
                              BuildStatusTable(ModelTypeSet(),
                                               ModelTypeSet(BOOKMARKS),
                                               ModelTypeSet(),
                                               ModelTypeSet()));
  Configure(dtm_.get(), ModelTypeSet(BOOKMARKS));
  FinishDownload(*dtm_, ModelTypeSet(), ModelTypeSet());
  FinishDownload(*dtm_, ModelTypeSet(BOOKMARKS), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());

  EXPECT_EQ(0U, configurer_.activated_types().Size());
}


TEST_F(SyncDataTypeManagerImplTest, ErrorBeforeAssociation) {
  AddController(BOOKMARKS);

  // Bookmarks is never started due to hitting a datatype error while the DTM
  // is still downloading types.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK,
                              BuildStatusTable(ModelTypeSet(),
                                               ModelTypeSet(BOOKMARKS),
                                               ModelTypeSet(),
                                               ModelTypeSet()));
  Configure(dtm_.get(), ModelTypeSet(BOOKMARKS));
  FinishDownload(*dtm_, ModelTypeSet(), ModelTypeSet());
  GetController(BOOKMARKS)->OnSingleDataTypeUnrecoverableError(
      syncer::SyncError(FROM_HERE,
                        SyncError::DATATYPE_ERROR,
                        "bookmarks error",
                        BOOKMARKS));
  FinishDownload(*dtm_, ModelTypeSet(BOOKMARKS), ModelTypeSet());
  FinishDownload(*dtm_, ModelTypeSet(), ModelTypeSet());  // Reconfig for error.
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());

  EXPECT_EQ(0U, configurer_.activated_types().Size());
}

TEST_F(SyncDataTypeManagerImplTest, AssociationNeverCompletes) {
  AddController(BOOKMARKS);

  // Bookmarks times out during association and so it's never started.
  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK,
                              BuildStatusTable(ModelTypeSet(),
                                               ModelTypeSet(BOOKMARKS),
                                               ModelTypeSet(),
                                               ModelTypeSet()));
  Configure(dtm_.get(), ModelTypeSet(BOOKMARKS));

  GetController(BOOKMARKS)->SetDelayModelLoad();
  FinishDownload(*dtm_, ModelTypeSet(), ModelTypeSet());
  FinishDownload(*dtm_, ModelTypeSet(BOOKMARKS), ModelTypeSet());

  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Simulate timeout by firing the timer.
  dtm_->GetModelAssociationManagerForTesting()
      ->GetTimerForTesting()
      ->user_task()
      .Run();
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(DataTypeController::NOT_RUNNING, GetController(BOOKMARKS)->state());

  FinishDownload(*dtm_, ModelTypeSet(), ModelTypeSet());

  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(0U, configurer_.activated_types().Size());
}

// Test that sync configures properly if all low priority types are ready.
TEST_F(SyncDataTypeManagerImplTest, AllLowPriorityTypesReady) {
  AddController(PREFERENCES);
  AddController(BOOKMARKS);

  dtm_->set_priority_types(
      AddControlTypesTo(ModelTypeSet(PREFERENCES)));

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  Configure(dtm_.get(), ModelTypeSet(PREFERENCES, BOOKMARKS));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  configurer_.set_ready_types(ModelTypeSet(BOOKMARKS));
  FinishDownload(*dtm_, ModelTypeSet(PREFERENCES), ModelTypeSet());

  // Association of Bookmarks can't happen until higher priority types are
  // finished.
  EXPECT_EQ(DataTypeController::ASSOCIATING,
            GetController(PREFERENCES)->state());
  EXPECT_EQ(DataTypeController::MODEL_LOADED,
            GetController(BOOKMARKS)->state());

  // Because Bookmarks are a ready type, once Preference finishes, Bookmarks
  // can start associating immediately (even before the
  // BackendDataTypeConfigurer calls back).
  GetController(PREFERENCES)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeController::ASSOCIATING,
            GetController(BOOKMARKS)->state());

  // Once the association finishes, the DTM should still be waiting for the
  // Sync configurer to call back.
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeController::RUNNING,
            GetController(BOOKMARKS)->state());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Finishing the download should complete the configuration.
  FinishDownload(*dtm_, ModelTypeSet(BOOKMARKS), ModelTypeSet());
  EXPECT_EQ(DataTypeController::RUNNING,
            GetController(BOOKMARKS)->state());
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(2U, configurer_.activated_types().Size());

  dtm_->Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
  EXPECT_TRUE(configurer_.activated_types().Empty());
}

// Test that sync configures properly if all high priority types are ready.
TEST_F(SyncDataTypeManagerImplTest, AllHighPriorityTypesReady) {
  AddController(PREFERENCES);
  AddController(BOOKMARKS);

  dtm_->set_priority_types(
      AddControlTypesTo(ModelTypeSet(PREFERENCES)));

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  configurer_.set_ready_types(ModelTypeSet(PREFERENCES));
  Configure(dtm_.get(), ModelTypeSet(PREFERENCES, BOOKMARKS));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Association of Bookmarks can't happen until higher priority types are
  // finished, but Preferences should start associating immediately.
  EXPECT_EQ(DataTypeController::ASSOCIATING,
            GetController(PREFERENCES)->state());
  EXPECT_EQ(DataTypeController::MODEL_LOADED,
            GetController(BOOKMARKS)->state());

  // When Prefs finish associating, configuration should still be waiting for
  // the high priority download to finish.
  GetController(PREFERENCES)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeController::MODEL_LOADED,
            GetController(BOOKMARKS)->state());

  // Because Bookmarks aren't a ready type, they'll need to wait until the
  // low priority download also finishes.
  FinishDownload(*dtm_, ModelTypeSet(PREFERENCES), ModelTypeSet());
  EXPECT_EQ(DataTypeController::MODEL_LOADED,
            GetController(BOOKMARKS)->state());

  FinishDownload(*dtm_, ModelTypeSet(BOOKMARKS), ModelTypeSet());
  EXPECT_EQ(DataTypeController::ASSOCIATING,
            GetController(BOOKMARKS)->state());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Finishing the Bookmarks association ends the configuration.
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeController::RUNNING,
            GetController(BOOKMARKS)->state());

  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(2U, configurer_.activated_types().Size());

  dtm_->Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
  EXPECT_TRUE(configurer_.activated_types().Empty());
}

// Test that sync configures properly if all types are ready.
TEST_F(SyncDataTypeManagerImplTest, AllTypesReady) {
  AddController(PREFERENCES);
  AddController(BOOKMARKS);

  dtm_->set_priority_types(
      AddControlTypesTo(ModelTypeSet(PREFERENCES)));

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  configurer_.set_ready_types(ModelTypeSet(PREFERENCES));
  Configure(dtm_.get(), ModelTypeSet(PREFERENCES, BOOKMARKS));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Association of Bookmarks can't happen until higher priority types are
  // finished, but Preferences should start associating immediately.
  EXPECT_EQ(DataTypeController::ASSOCIATING,
            GetController(PREFERENCES)->state());
  EXPECT_EQ(DataTypeController::MODEL_LOADED,
            GetController(BOOKMARKS)->state());

  // When Prefs finish associating, configuration should still be waiting for
  // the high priority download to finish.
  GetController(PREFERENCES)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeController::MODEL_LOADED,
            GetController(BOOKMARKS)->state());

  // Because Bookmarks are a ready type, it can start associating immediately
  // after the high priority types finish downloading.
  configurer_.set_ready_types(ModelTypeSet(BOOKMARKS));
  FinishDownload(*dtm_, ModelTypeSet(PREFERENCES), ModelTypeSet());
  EXPECT_EQ(DataTypeController::ASSOCIATING,
            GetController(BOOKMARKS)->state());

  // Finishing the Bookmarks association leaves the DTM waiting for the low
  // priority download to finish.
  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeController::RUNNING,
            GetController(BOOKMARKS)->state());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Finishing the low priority download ends the configuration.
  FinishDownload(*dtm_, ModelTypeSet(BOOKMARKS), ModelTypeSet());

  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(2U, configurer_.activated_types().Size());

  dtm_->Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
  EXPECT_TRUE(configurer_.activated_types().Empty());
}

// Test that "catching up" type puts them in the CONFIGURE_CLEAN state.
TEST_F(SyncDataTypeManagerImplTest, CatchUpTypeAddedToConfigureClean) {
  AddController(BOOKMARKS);
  AddController(PASSWORDS);

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());
  configurer_.set_expected_configure_types(
      BackendDataTypeConfigurer::CONFIGURE_CLEAN,
      AddControlTypesTo(ModelTypeSet(BOOKMARKS, PASSWORDS)));

  dtm_->Configure(ModelTypeSet(BOOKMARKS, PASSWORDS),
                  syncer::CONFIGURE_REASON_CATCH_UP);
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  FinishDownload(*dtm_, ModelTypeSet(), ModelTypeSet());
  FinishDownload(*dtm_, ModelTypeSet(BOOKMARKS, PASSWORDS), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  EXPECT_EQ(1U, configurer_.activated_types().Size());
  GetController(PASSWORDS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(2U, configurer_.activated_types().Size());

  dtm_->Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
  EXPECT_TRUE(configurer_.activated_types().Empty());
}

// Test that once we start a catch up cycle for a type, the type ends up in the
// clean state and DataTypeManager remains in catch up mode for subsequent
// overlapping cycles.
TEST_F(SyncDataTypeManagerImplTest, CatchUpMultipleConfigureCalls) {
  AddController(BOOKMARKS);
  AddController(PASSWORDS);

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  // Configure (catch up) with one type.
  configurer_.set_expected_configure_types(
      BackendDataTypeConfigurer::CONFIGURE_CLEAN,
      AddControlTypesTo(ModelTypeSet(BOOKMARKS)));
  dtm_->Configure(ModelTypeSet(BOOKMARKS), syncer::CONFIGURE_REASON_CATCH_UP);
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  // Configure with both types before the first one completes. Both types should
  // end up in CONFIGURE_CLEAN.
  configurer_.set_expected_configure_types(
      BackendDataTypeConfigurer::CONFIGURE_CLEAN,
      AddControlTypesTo(ModelTypeSet(BOOKMARKS, PASSWORDS)));
  dtm_->Configure(ModelTypeSet(BOOKMARKS, PASSWORDS),
                  syncer::CONFIGURE_REASON_RECONFIGURATION);
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  FinishDownload(*dtm_, ModelTypeSet(), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  FinishDownload(*dtm_, ModelTypeSet(), ModelTypeSet());
  FinishDownload(*dtm_, ModelTypeSet(BOOKMARKS, PASSWORDS), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  GetController(PASSWORDS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());

  dtm_->Stop();
  EXPECT_EQ(DataTypeManager::STOPPED, dtm_->state());
}

// Test that DataTypeManagerImpl delays configuration until all datatypes for
//  which ShouldLoadModelBeforeConfigure() returns true loaded their models.
TEST_F(SyncDataTypeManagerImplTest, DelayConfigureForUSSTypes) {
  AddController(BOOKMARKS);
  GetController(BOOKMARKS)->SetShouldLoadModelBeforeConfigure(true);
  GetController(BOOKMARKS)->SetDelayModelLoad();

  SetConfigureStartExpectation();
  SetConfigureDoneExpectation(DataTypeManager::OK, DataTypeStatusTable());

  Configure(dtm_.get(), ModelTypeSet(BOOKMARKS));
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());
  // Bookmarks model isn't loaded yet and it is required to complete before
  // call to configure. Ensure that configure wasn't called.
  EXPECT_EQ(0, configurer_.configure_call_count());
  EXPECT_EQ(0, GetController(BOOKMARKS)->register_with_backend_call_count());

  // Finishing model load should trigger configure.
  GetController(BOOKMARKS)->SimulateModelLoadFinishing();
  EXPECT_EQ(1, configurer_.configure_call_count());
  EXPECT_EQ(1, GetController(BOOKMARKS)->register_with_backend_call_count());

  FinishDownload(*dtm_, ModelTypeSet(), ModelTypeSet());
  FinishDownload(*dtm_, ModelTypeSet(BOOKMARKS), ModelTypeSet());
  EXPECT_EQ(DataTypeManager::CONFIGURING, dtm_->state());

  GetController(BOOKMARKS)->FinishStart(DataTypeController::OK);
  EXPECT_EQ(DataTypeManager::CONFIGURED, dtm_->state());
  EXPECT_EQ(1U, configurer_.activated_types().Size());
}

}  // namespace sync_driver
