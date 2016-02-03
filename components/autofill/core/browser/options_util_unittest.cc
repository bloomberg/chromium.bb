// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/options_util.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/testing_pref_service.h"
#include "components/sync_driver/data_type_manager.h"
#include "components/sync_driver/fake_sync_service.h"
#include "components/sync_driver/sync_service.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "sync/internal_api/public/engine/sync_status.h"
#include "sync/internal_api/public/sessions/sync_session_snapshot.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::Return;

namespace autofill {

namespace {

class TestSyncService : public sync_driver::FakeSyncService {
 public:
  TestSyncService(syncer::ModelTypeSet type_set,
                  bool can_sync_start)
      : type_set_(type_set),
        can_sync_start_(can_sync_start) {}
  ~TestSyncService() override {}

  // FakeSyncService overrides.
  bool IsSyncAllowed() const override { return true; }
  syncer::ModelTypeSet GetActiveDataTypes() const override {
    return type_set_;
  }
  syncer::ModelTypeSet GetPreferredDataTypes() const override {
    return type_set_;
  }
  bool CanSyncStart() const override { return can_sync_start_; }

 private:
  syncer::ModelTypeSet type_set_;
  bool can_sync_start_;
};

scoped_ptr<TestSyncService> CreateSyncService(bool has_autofill_profile,
                                              bool is_enabled_and_logged_in) {
  syncer::ModelTypeSet type_set;
  if (has_autofill_profile)
    type_set.Put(syncer::AUTOFILL_PROFILE);
  return make_scoped_ptr(
      new TestSyncService(type_set, is_enabled_and_logged_in));
}

scoped_ptr<TestingPrefServiceSimple> CreatePrefService(
    bool autofill_wallet_import_enabled,
    bool autofill_wallet_sync_experiment_enabled) {
  scoped_ptr<TestingPrefServiceSimple> prefs(new TestingPrefServiceSimple());
  prefs->registry()->RegisterBooleanPref(prefs::kAutofillEnabled, true);
  prefs->registry()->RegisterBooleanPref(prefs::kAutofillWalletImportEnabled,
                                         autofill_wallet_import_enabled);
  prefs->registry()->RegisterBooleanPref(
      prefs::kAutofillWalletSyncExperimentEnabled,
      autofill_wallet_sync_experiment_enabled);

  return prefs;
}

scoped_ptr<TestPersonalDataManager> CreatePersonalDataManager(
    PrefService* prefs) {
  scoped_ptr<TestPersonalDataManager> pdm(new TestPersonalDataManager());
  pdm->SetTestingPrefService(prefs);
  return pdm;
}

}  // namespace

// Verify that true is returned when all inputs are complete.
TEST(WalletIntegrationAvailableTest, AllInputsComplete) {
  scoped_ptr<TestSyncService> sync = CreateSyncService(true, true);
  scoped_ptr<TestingPrefServiceSimple> prefs = CreatePrefService(true, true);
  scoped_ptr<TestPersonalDataManager> pdm =
      CreatePersonalDataManager(prefs.get());

  EXPECT_TRUE(WalletIntegrationAvailable(sync.get(), *pdm));
}

// Verify that true is returned even if wallet import is disabled. (Otherwise
// the user will never be able to enable it).
TEST(WalletIntegrationAvailableTest, WalletImportDisabled) {
  scoped_ptr<TestSyncService> sync = CreateSyncService(true, true);
  scoped_ptr<TestingPrefServiceSimple> prefs = CreatePrefService(false, true);
  scoped_ptr<TestPersonalDataManager> pdm =
      CreatePersonalDataManager(prefs.get());

  EXPECT_TRUE(WalletIntegrationAvailable(sync.get(), *pdm));
}

// Verify that false is returned when SyncService is missing or incomplete.
TEST(WalletIntegrationAvailableTest, MissingOrIncompleteSyncService) {
  // Setup |prefs| and |pdm| to do their part to make
  // WalletIntegrationAvailable() return true.
  scoped_ptr<TestingPrefServiceSimple> prefs = CreatePrefService(true, true);
  scoped_ptr<TestPersonalDataManager> pdm =
      CreatePersonalDataManager(prefs.get());

  // Incomplete SyncService data should return false.
  EXPECT_FALSE(WalletIntegrationAvailable(NULL, *pdm));

  scoped_ptr<TestSyncService> sync = CreateSyncService(false, false);
  EXPECT_FALSE(WalletIntegrationAvailable(sync.get(), *pdm));

  sync = CreateSyncService(false, true);
  EXPECT_FALSE(WalletIntegrationAvailable(sync.get(), *pdm));

  sync = CreateSyncService(true, false);
  EXPECT_FALSE(WalletIntegrationAvailable(sync.get(), *pdm));

  // Complete SyncService data should return true.
  sync = CreateSyncService(true, true);
  EXPECT_TRUE(WalletIntegrationAvailable(sync.get(), *pdm));
}

// Verify that false is returned when
// !prefs::kAutofillWalletSyncExperimentEnabled.
TEST(WalletIntegrationAvailableTest, ExperimentalWalletIntegrationDisabled) {
  scoped_ptr<TestSyncService> sync = CreateSyncService(true, true);
  // Set kAutofillWalletSyncExperimentEnabled to false.
  scoped_ptr<TestingPrefServiceSimple> prefs = CreatePrefService(true, false);
  scoped_ptr<TestPersonalDataManager> pdm =
      CreatePersonalDataManager(prefs.get());

  EXPECT_FALSE(WalletIntegrationAvailable(sync.get(), *pdm));
}

}  // namespace autofill
