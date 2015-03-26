// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/sync/profile_sync_service.h"
#include "chrome/browser/sync/test/integration/autofill_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/single_client_status_change_checker.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "content/public/browser/notification_service.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/test/fake_server/fake_server_entity.h"
#include "sync/test/fake_server/unique_client_entity.h"

using autofill_helper::GetPersonalDataManager;
using sync_integration_test_util::AwaitCommitActivityCompletion;

namespace {

// Setting the Preferences files contents to this string (before Profile is
// created) will bypass the Sync experiment logic for enabling this feature.
const char kWalletSyncEnabledPreferencesContents[] =
    "{\"autofill\": { \"wallet_import_sync_experiment_enabled\": true } }";

const char kWalletSyncExperimentTag[] = "wallet_sync";

}  // namespace

class SingleClientWalletSyncTest : public SyncTest {
 public:
  SingleClientWalletSyncTest() : SyncTest(SINGLE_CLIENT) {}
  ~SingleClientWalletSyncTest() override {}

  void TriggerSyncCycle() {
    // Note: we use the experiments type here as we want to be able to trigger a
    // sync cycle even when wallet is not enabled yet.
    const syncer::ModelTypeSet kExperimentsType(syncer::EXPERIMENTS);
    content::NotificationService::current()->Notify(
        chrome::NOTIFICATION_SYNC_REFRESH_LOCAL,
        content::Source<Profile>(GetProfile(0)),
        content::Details<const syncer::ModelTypeSet>(&kExperimentsType));
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientWalletSyncTest);
};

// Checker that will wait until an asynchronous Wallet datatype enable event
// happens, or times out.
class WalletEnabledChecker : public SingleClientStatusChangeChecker {
 public:
  WalletEnabledChecker()
      : SingleClientStatusChangeChecker(
            sync_datatype_helper::test()->GetSyncService(0)) {}
  ~WalletEnabledChecker() override {}

  // SingleClientStatusChangeChecker overrides.
  bool IsExitConditionSatisfied() override {
    return service()->GetActiveDataTypes().Has(syncer::AUTOFILL_WALLET_DATA);
  }
  std::string GetDebugMessage() const override {
    return "Waiting for wallet enable event.";
  }
};

// Checker that will wait until an asynchronous Wallet datatype disable event
// happens, or times out
class WalletDisabledChecker : public SingleClientStatusChangeChecker {
 public:
  WalletDisabledChecker()
      : SingleClientStatusChangeChecker(
            sync_datatype_helper::test()->GetSyncService(0)) {}
  ~WalletDisabledChecker() override {}

  // SingleClientStatusChangeChecker overrides.
  bool IsExitConditionSatisfied() override {
    return !service()->GetActiveDataTypes().Has(syncer::AUTOFILL_WALLET_DATA);
  }
  std::string GetDebugMessage() const override {
    return "Waiting for wallet disable event.";
  }
};

IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest, DisabledByDefault) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed";
  // The type should not be enabled without the experiment enabled.
  ASSERT_FALSE(GetClient(0)->service()->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_DATA));
}

IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest, EnabledViaPreference) {
  SetPreexistingPreferencesFileContents(kWalletSyncEnabledPreferencesContents);
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed";
  // The type should not be enabled without the experiment enabled.
  ASSERT_TRUE(GetClient(0)->service()->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_DATA));
  // TODO(pvalenzuela): Assert that the local root node for AUTOFILL_WALLET_DATA
  // exists.
}

// Tests that an experiment received at sync startup time (during sign-in)
// enables the wallet datatype.
IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest,
                       EnabledViaExperimentStartup) {
  sync_pb::EntitySpecifics experiment_entity;
  sync_pb::ExperimentsSpecifics* experiment_specifics =
      experiment_entity.mutable_experiments();
  experiment_specifics->mutable_wallet_sync()->set_enabled(true);
  GetFakeServer()->InjectEntity(
      fake_server::UniqueClientEntity::CreateForInjection(
          syncer::EXPERIMENTS,
          kWalletSyncExperimentTag,
          experiment_entity));

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed";
  ASSERT_TRUE(GetClient(0)->service()->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_DATA));
}

// Tests receiving an enable experiment at runtime, followed by a disabled
// experiment, and verifies the datatype is enabled/disabled as necessary.
IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest,
                       EnabledDisabledViaExperiment) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed";
  ASSERT_FALSE(GetClient(0)->service()->GetActiveDataTypes().
      Has(syncer::AUTOFILL_WALLET_DATA));

  sync_pb::EntitySpecifics experiment_entity;
  sync_pb::ExperimentsSpecifics* experiment_specifics =
      experiment_entity.mutable_experiments();

  // First enable the experiment.
  experiment_specifics->mutable_wallet_sync()->set_enabled(true);
  GetFakeServer()->InjectEntity(
      fake_server::UniqueClientEntity::CreateForInjection(
          syncer::EXPERIMENTS, kWalletSyncExperimentTag, experiment_entity));
  TriggerSyncCycle();

  WalletEnabledChecker enabled_checker;
  enabled_checker.Wait();
  ASSERT_FALSE(enabled_checker.TimedOut());
  ASSERT_TRUE(GetClient(0)->service()->GetActiveDataTypes().Has(
      syncer::AUTOFILL_WALLET_DATA));

  // Then disable the experiment.
  experiment_specifics->mutable_wallet_sync()->set_enabled(false);
  GetFakeServer()->InjectEntity(
      fake_server::UniqueClientEntity::CreateForInjection(
          syncer::EXPERIMENTS, kWalletSyncExperimentTag, experiment_entity));
  TriggerSyncCycle();

  WalletDisabledChecker disable_checker;
  disable_checker.Wait();
  ASSERT_FALSE(disable_checker.TimedOut());
  ASSERT_FALSE(GetClient(0)->service()->GetActiveDataTypes().
      Has(syncer::AUTOFILL_WALLET_DATA));
}

IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest, Download) {
  SetPreexistingPreferencesFileContents(kWalletSyncEnabledPreferencesContents);

  std::string id = "wallet entity ID";
  int expiration_month = 8;
  int expiration_year = 2087;
  std::string last_four_digits = "1234";
  std::string name_on_card = "Patrick Valenzuela";

  sync_pb::EntitySpecifics specifics;
  sync_pb::AutofillWalletSpecifics* wallet_specifics =
      specifics.mutable_autofill_wallet();
  wallet_specifics->set_type(
      sync_pb::AutofillWalletSpecifics::MASKED_CREDIT_CARD);

  sync_pb::WalletMaskedCreditCard* credit_card =
      wallet_specifics->mutable_masked_card();
  credit_card->set_id(id);
  credit_card->set_exp_month(expiration_month);
  credit_card->set_exp_year(expiration_year);
  credit_card->set_last_four(last_four_digits);
  credit_card->set_name_on_card(name_on_card);
  credit_card->set_status(sync_pb::WalletMaskedCreditCard::VALID);
  credit_card->set_type(sync_pb::WalletMaskedCreditCard::AMEX);

  GetFakeServer()->InjectEntity(
      fake_server::UniqueClientEntity::CreateForInjection(
          syncer::AUTOFILL_WALLET_DATA,
          id,
          specifics));

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed";

  autofill::PersonalDataManager* pdm = GetPersonalDataManager(0);
  ASSERT_TRUE(pdm != nullptr);
  std::vector<autofill::CreditCard*> cards = pdm->GetCreditCards();
  ASSERT_EQ(1uL, cards.size());

  autofill::CreditCard* card = cards[0];
  ASSERT_EQ(autofill::CreditCard::MASKED_SERVER_CARD, card->record_type());
  ASSERT_EQ(id, card->server_id());
  ASSERT_EQ(base::UTF8ToUTF16(last_four_digits), card->LastFourDigits());
  ASSERT_EQ(autofill::kAmericanExpressCard, card->type());
  ASSERT_EQ(expiration_month, card->expiration_month());
  ASSERT_EQ(expiration_year, card->expiration_year());
  ASSERT_EQ(base::UTF8ToUTF16(name_on_card),
            card->GetRawInfo(autofill::ServerFieldType::CREDIT_CARD_NAME));
}
