// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/prefs/pref_registry_simple.h"
#include "base/prefs/testing_pref_service.h"
#include "components/autofill/core/browser/autofill_test_utils.h"
#include "components/autofill/core/browser/options_util.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "components/autofill/core/common/autofill_pref_names.h"
#include "components/sync_driver/data_type_manager.h"
#include "components/sync_driver/sync_service.h"
#include "google_apis/gaia/google_service_auth_error.h"
#include "testing/gmock/include/gmock/gmock.h"

using testing::Return;

namespace autofill {

namespace {

class SyncServiceMock : public sync_driver::SyncService {
 public:
  MOCK_CONST_METHOD0(HasSyncSetupCompleted, bool());
  MOCK_CONST_METHOD0(IsSyncActive, bool());
  MOCK_CONST_METHOD0(IsSyncAllowed, bool());
  MOCK_CONST_METHOD0(GetActiveDataTypes, syncer::ModelTypeSet());
  MOCK_METHOD1(AddObserver, void(sync_driver::SyncServiceObserver*));
  MOCK_METHOD1(RemoveObserver, void(sync_driver::SyncServiceObserver*));
  MOCK_CONST_METHOD1(HasObserver,
                     bool(const sync_driver::SyncServiceObserver*));
  MOCK_CONST_METHOD0(CanSyncStart, bool());
  MOCK_METHOD0(DisableForUser, void());
  MOCK_METHOD0(RequestStop, void());
  MOCK_METHOD0(RequestStart, void());
  MOCK_CONST_METHOD0(GetPreferredDataTypes, syncer::ModelTypeSet());
  MOCK_METHOD2(OnUserChoseDatatypes,
               void(bool sync_everything, syncer::ModelTypeSet chosen_types));
  MOCK_METHOD0(SetSyncSetupCompleted, void());
  MOCK_CONST_METHOD0(FirstSetupInProgress, bool());
  MOCK_METHOD1(SetSetupInProgress, void(bool));
  MOCK_CONST_METHOD0(setup_in_progress, bool());
  MOCK_CONST_METHOD0(ConfigurationDone, bool());
  MOCK_CONST_METHOD0(GetAuthError, const GoogleServiceAuthError&());
  MOCK_CONST_METHOD0(HasUnrecoverableError, bool());
  MOCK_CONST_METHOD0(backend_initialized, bool());
  MOCK_CONST_METHOD0(IsPassphraseRequiredForDecryption, bool());
  MOCK_CONST_METHOD0(GetExplicitPassphraseTime, base::Time());
  MOCK_CONST_METHOD0(IsUsingSecondaryPassphrase, bool());
  MOCK_METHOD0(EnableEncryptEverything, void());
  MOCK_METHOD2(SetEncryptionPassphrase,
               void(const std::string& passphrase, PassphraseType type));
  MOCK_METHOD1(SetDecryptionPassphrase, bool(const std::string& passphrase));

  // DataTypeEncryptionHandler mocks.
  MOCK_CONST_METHOD0(IsPassphraseRequired, bool());
  MOCK_CONST_METHOD0(GetEncryptedDataTypes, syncer::ModelTypeSet());
};

scoped_ptr<SyncServiceMock> CreateSyncService(bool has_autofill_profile,
                                              bool has_autofill_wallet_data,
                                              bool is_enabled_and_logged_in) {
  scoped_ptr<SyncServiceMock> sync(new SyncServiceMock());

  ON_CALL(*sync, IsSyncAllowed()).WillByDefault(Return(true));

  syncer::ModelTypeSet type_set;
  if (has_autofill_profile)
    type_set.Put(syncer::AUTOFILL_PROFILE);
  if (has_autofill_wallet_data)
    type_set.Put(syncer::AUTOFILL_WALLET_DATA);
  ON_CALL(*sync, GetActiveDataTypes()).WillByDefault(Return(type_set));
  ON_CALL(*sync, GetPreferredDataTypes()).WillByDefault(Return(type_set));

  ON_CALL(*sync, CanSyncStart())
      .WillByDefault(Return(is_enabled_and_logged_in));

  return sync;
}

scoped_ptr<TestingPrefServiceSimple> CreatePrefService(
    bool autofill_enabled,
    bool autofill_wallet_import_enabled,
    bool autofill_wallet_sync_experiment_enabled) {
  scoped_ptr<TestingPrefServiceSimple> prefs(new TestingPrefServiceSimple());
  prefs->registry()->RegisterBooleanPref(prefs::kAutofillEnabled,
                                         autofill_enabled);
  prefs->registry()->RegisterBooleanPref(prefs::kAutofillWalletImportEnabled,
                                         autofill_wallet_import_enabled);
  prefs->registry()->RegisterBooleanPref(
      prefs::kAutofillWalletSyncExperimentEnabled,
      autofill_wallet_sync_experiment_enabled);

  return prefs;
}

scoped_ptr<TestPersonalDataManager> CreatePersonalDataManager(
    PrefService* prefs,
    bool has_server_data) {
  scoped_ptr<TestPersonalDataManager> pdm(new TestPersonalDataManager());
  pdm->SetTestingPrefService(prefs);
  if (has_server_data) {
    // This will cause pdm->HasServerData() to return true.
    pdm->AddTestingServerCreditCard(test::GetVerifiedCreditCard());
  }

  return pdm;
}

}  // namespace

// Verify that true is returned when all inputs are complete.
TEST(WalletIntegrationAvailableTest, AllInputsComplete) {
  scoped_ptr<SyncServiceMock> sync = CreateSyncService(true, true, true);
  scoped_ptr<TestingPrefServiceSimple> prefs =
      CreatePrefService(true, true, true);
  scoped_ptr<TestPersonalDataManager> pdm =
      CreatePersonalDataManager(prefs.get(), true);

  EXPECT_TRUE(WalletIntegrationAvailable(sync.get(), *prefs, *pdm));
}

// Verify that false is returned when SyncService is missing or incomplete.
TEST(WalletIntegrationAvailableTest, MissingOrIncompleteSyncService) {
  // Setup |prefs| and |pdm| to do their part to make
  // WalletIntegrationAvailable() return true.
  scoped_ptr<TestingPrefServiceSimple> prefs =
      CreatePrefService(true, true, true);
  scoped_ptr<TestPersonalDataManager> pdm =
      CreatePersonalDataManager(prefs.get(), true);

  // Incomplete SyncService data should return false.
  EXPECT_FALSE(WalletIntegrationAvailable(NULL, *prefs, *pdm));

  scoped_ptr<SyncServiceMock> sync = CreateSyncService(false, false, false);
  EXPECT_FALSE(WalletIntegrationAvailable(sync.get(), *prefs, *pdm));

  sync = CreateSyncService(false, false, true);
  EXPECT_FALSE(WalletIntegrationAvailable(sync.get(), *prefs, *pdm));

  sync = CreateSyncService(true, false, false);
  EXPECT_FALSE(WalletIntegrationAvailable(sync.get(), *prefs, *pdm));

  // Complete SyncService data should return true.
  sync = CreateSyncService(true, true, true);
  EXPECT_TRUE(WalletIntegrationAvailable(sync.get(), *prefs, *pdm));
}

// Verify that false is returned when
// !prefs::kAutofillWalletSyncExperimentEnabled.
TEST(WalletIntegrationAvailableTest, ExperimentalWalletIntegrationDisabled) {
  scoped_ptr<SyncServiceMock> sync = CreateSyncService(true, true, true);
  // Set kAutofillWalletSyncExperimentEnabled to false.
  scoped_ptr<TestingPrefServiceSimple> prefs =
      CreatePrefService(true, true, false);
  scoped_ptr<TestPersonalDataManager> pdm =
      CreatePersonalDataManager(prefs.get(), true);

  EXPECT_FALSE(WalletIntegrationAvailable(sync.get(), *prefs, *pdm));
}

// Verify that false is returned if server data is missing.
TEST(WalletIntegrationAvailableTest, NoServerData) {
  scoped_ptr<SyncServiceMock> sync = CreateSyncService(true, true, true);
  scoped_ptr<TestingPrefServiceSimple> prefs =
      CreatePrefService(true, true, true);
  // Set server data as missing.
  scoped_ptr<TestPersonalDataManager> pdm =
      CreatePersonalDataManager(prefs.get(), false);

  EXPECT_FALSE(WalletIntegrationAvailable(sync.get(), *prefs, *pdm));
}

// Verify that true is returned when !prefs::kAutofillWalletImportEnabled,
// even if server data is missing.
TEST(WalletIntegrationAvailableTest, WalletImportDisabled) {
  scoped_ptr<SyncServiceMock> sync = CreateSyncService(true, true, true);
  // Set kAutofillWalletImportEnabled to false.
  scoped_ptr<TestingPrefServiceSimple> prefs =
      CreatePrefService(true, false, true);
  // Set server data as missing.
  scoped_ptr<TestPersonalDataManager> pdm =
      CreatePersonalDataManager(prefs.get(), false);

  EXPECT_TRUE(WalletIntegrationAvailable(sync.get(), *prefs, *pdm));
}

// Verify that true is returned when data hasn't been synced yet, even if
// server data is missing.
TEST(WalletIntegrationAvailableTest, WalletDataNotSyncedYet) {
  // Set wallet data as not synced yet.
  scoped_ptr<SyncServiceMock> sync = CreateSyncService(true, false, true);
  scoped_ptr<TestingPrefServiceSimple> prefs =
      CreatePrefService(true, true, true);
  // Set server data as missing.
  scoped_ptr<TestPersonalDataManager> pdm =
      CreatePersonalDataManager(prefs.get(), false);

  EXPECT_TRUE(WalletIntegrationAvailable(sync.get(), *prefs, *pdm));
}

}  // namespace autofill
