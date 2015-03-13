// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "sync/internal_api/public/base/model_type.h"
#include "sync/test/fake_server/fake_server_entity.h"
#include "sync/test/fake_server/unique_client_entity.h"

using sync_integration_test_util::AwaitCommitActivityCompletion;

namespace {

// Setting the Preferences files contents to this string (before Profile is
// created) will bypass the Sync experiment logic for enabling this feature.
const char kWalletSyncEnabledPreferencesContents[] =
    "{\"autofill\": { \"wallet_import_sync_experiment_enabled\": true } }";

}  // namespace

class SingleClientWalletSyncTest : public SyncTest {
 public:
  SingleClientWalletSyncTest() : SyncTest(SINGLE_CLIENT) {}
  ~SingleClientWalletSyncTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(SingleClientWalletSyncTest);
};

IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest, DisabledByDefault) {
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed";
  // The type should not be enabled without the experiment enabled.
  ASSERT_FALSE(GetClient(0)->IsTypePreferred(syncer::AUTOFILL_WALLET_DATA));
}

IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest, EnabledViaPreference) {
  SetPreexistingPreferencesFileContents(kWalletSyncEnabledPreferencesContents);
  ASSERT_TRUE(SetupSync()) << "SetupSync() failed";
  // The type should not be enabled without the experiment enabled.
  ASSERT_TRUE(GetClient(0)->IsTypePreferred(syncer::AUTOFILL_WALLET_DATA));
  // TODO(pvalenzuela): Assert that the local root node for AUTOFILL_WALLET_DATA
  // exists.
}

IN_PROC_BROWSER_TEST_F(SingleClientWalletSyncTest, Download) {
  SetPreexistingPreferencesFileContents(kWalletSyncEnabledPreferencesContents);

  std::string id = "wallet entity ID";
  sync_pb::EntitySpecifics specifics;
  sync_pb::AutofillWalletSpecifics* wallet_specifics =
      specifics.mutable_autofill_wallet();
  wallet_specifics->set_type(
      sync_pb::AutofillWalletSpecifics::MASKED_CREDIT_CARD);

  sync_pb::WalletMaskedCreditCard* credit_card =
      wallet_specifics->mutable_masked_card();
  credit_card->set_id(id);
  credit_card->set_exp_month(8);
  credit_card->set_exp_year(2087);
  credit_card->set_last_four("1234");
  credit_card->set_name_on_card("Patrick Valenzuela");
  credit_card->set_status(sync_pb::WalletMaskedCreditCard::VALID);
  credit_card->set_type(sync_pb::WalletMaskedCreditCard::AMEX);

  GetFakeServer()->InjectEntity(
      fake_server::UniqueClientEntity::CreateForInjection(
          syncer::AUTOFILL_WALLET_DATA,
          id,
          specifics));

  ASSERT_TRUE(SetupSync()) << "SetupSync() failed";
  // TODO(pvalenzuela): Assert that the credit card entity has been synced down.
}
