// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/sync/test/integration/autofill_helper.h"
#include "chrome/browser/sync/test/integration/profile_sync_service_harness.h"
#include "chrome/browser/sync/test/integration/sync_integration_test_util.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/personal_data_manager.h"
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
