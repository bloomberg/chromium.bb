// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_wallet_syncable_service.h"

#include <vector>

#include "base/memory/ptr_util.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/sync/protocol/autofill_specifics.pb.h"
#include "components/sync/protocol/sync.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

syncer::SyncData CreateSyncDataForWalletCreditCard(
    const std::string& id,
    const std::string& billing_address_id) {
  sync_pb::EntitySpecifics specifics;

  sync_pb::AutofillWalletSpecifics* wallet_specifics =
      specifics.mutable_autofill_wallet();
  wallet_specifics->set_type(
      sync_pb::AutofillWalletSpecifics_WalletInfoType::
          AutofillWalletSpecifics_WalletInfoType_MASKED_CREDIT_CARD);

  sync_pb::WalletMaskedCreditCard* card_specifics =
      wallet_specifics->mutable_masked_card();
  card_specifics->set_id(id);
  card_specifics->set_billing_address_id(billing_address_id);
  return syncer::SyncData::CreateLocalData(id, id, specifics);
}

syncer::SyncData CreateSyncDataForWalletAddress(const std::string& id) {
  sync_pb::EntitySpecifics specifics;

  sync_pb::AutofillWalletSpecifics* wallet_specifics =
      specifics.mutable_autofill_wallet();
  wallet_specifics->set_type(
      sync_pb::AutofillWalletSpecifics_WalletInfoType::
          AutofillWalletSpecifics_WalletInfoType_POSTAL_ADDRESS);

  sync_pb::WalletPostalAddress* address_specifics =
      wallet_specifics->mutable_address();
  address_specifics->set_id(id);
  return syncer::SyncData::CreateLocalData(id, id, specifics);
}

class TestAutofillTable : public AutofillTable {
 public:
  explicit TestAutofillTable(std::vector<CreditCard> cards_on_disk)
      : cards_on_disk_(cards_on_disk) {}

  ~TestAutofillTable() override {}

  bool GetServerCreditCards(
      std::vector<std::unique_ptr<CreditCard>>* cards) const override {
    for (const auto& card_on_disk : cards_on_disk_)
      cards->push_back(base::MakeUnique<CreditCard>(card_on_disk));
    return true;
  }

 private:
  std::vector<CreditCard> cards_on_disk_;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillTable);
};

}  // anonymous namespace

// Verify that the link between a card and its billing address from sync is
// present in the generated Autofill objects.
TEST(AutofillWalletSyncableServiceTest,
     PopulateWalletCardsAndAddresses_BillingAddressIdTransfer) {
  std::vector<CreditCard> wallet_cards;
  std::vector<AutofillProfile> wallet_addresses;
  syncer::SyncDataList data_list;

  // Create a Sync data for a card and its billing address.
  data_list.push_back(CreateSyncDataForWalletAddress("1" /* id */));
  data_list.push_back(CreateSyncDataForWalletCreditCard(
      "card1" /* id */, "1" /* billing_address_id */));

  AutofillWalletSyncableService::PopulateWalletCardsAndAddresses(
      data_list, &wallet_cards, &wallet_addresses);

  ASSERT_EQ(1U, wallet_cards.size());
  ASSERT_EQ(1U, wallet_addresses.size());

  // Make sure the card's billing address id is equal to the address' server id.
  EXPECT_EQ(wallet_addresses.back().server_id(),
            wallet_cards.back().billing_address_id());
}

// Verify that the billing address id from the card saved on disk is kept if it
// is a local profile guid.
TEST(AutofillWalletSyncableServiceTest,
     CopyRelevantMetadataFromDisk_KeepLocalAddresses) {
  std::vector<CreditCard> cards_on_disk;
  std::vector<CreditCard> wallet_cards;

  // Create a local profile to be used as a billing address.
  AutofillProfile billing_address;

  // Create a card on disk that refers to that local profile as its billing
  // address.
  cards_on_disk.push_back(CreditCard());
  cards_on_disk.back().set_billing_address_id(billing_address.guid());

  // Create a card pulled from wallet with the same id, but a different billing
  // address id.
  wallet_cards.push_back(CreditCard(cards_on_disk.back()));
  wallet_cards.back().set_billing_address_id("1234");

  // Setup the TestAutofillTable with the cards_on_disk.
  TestAutofillTable table(cards_on_disk);

  AutofillWalletSyncableService::CopyRelevantMetadataFromDisk(table,
                                                              &wallet_cards);

  ASSERT_EQ(1U, wallet_cards.size());

  // Make sure the wallet card replace its billind address id for the one that
  // was saved on disk.
  EXPECT_EQ(cards_on_disk.back().billing_address_id(),
            wallet_cards.back().billing_address_id());
}

// Verify that the billing address id from the card saved on disk is overwritten
// if it does not refer to a local profile.
TEST(AutofillWalletSyncableServiceTest,
     CopyRelevantMetadataFromDisk_OverwriteOtherAddresses) {
  std::string old_billing_id = "1234";
  std::string new_billing_id = "9876";
  std::vector<CreditCard> cards_on_disk;
  std::vector<CreditCard> wallet_cards;

  // Create a card on disk that does not refer to a local profile (which have 36
  // chars ids).
  cards_on_disk.push_back(CreditCard());
  cards_on_disk.back().set_billing_address_id(old_billing_id);

  // Create a card pulled from wallet with the same id, but a different billing
  // address id.
  wallet_cards.push_back(CreditCard(cards_on_disk.back()));
  wallet_cards.back().set_billing_address_id(new_billing_id);

  // Setup the TestAutofillTable with the cards_on_disk.
  TestAutofillTable table(cards_on_disk);

  AutofillWalletSyncableService::CopyRelevantMetadataFromDisk(table,
                                                              &wallet_cards);

  ASSERT_EQ(1U, wallet_cards.size());

  // Make sure the local address billing id that was saved on disk did not
  // replace the new one.
  EXPECT_EQ(new_billing_id, wallet_cards.back().billing_address_id());
}

// Verify that the use stats on disk are kept when server cards are synced.
TEST(AutofillWalletSyncableServiceTest,
     CopyRelevantMetadataFromDisk_KeepUseStats) {
  TestAutofillClock test_clock;
  base::Time arbitrary_time = base::Time::FromDoubleT(25);
  base::Time disk_time = base::Time::FromDoubleT(10);
  test_clock.SetNow(arbitrary_time);

  std::vector<CreditCard> cards_on_disk;
  std::vector<CreditCard> wallet_cards;

  // Create a card on disk with specific use stats.
  cards_on_disk.push_back(CreditCard());
  cards_on_disk.back().set_use_count(3U);
  cards_on_disk.back().set_use_date(disk_time);

  // Create a card pulled from wallet with the same id, but a different billing
  // address id.
  wallet_cards.push_back(CreditCard());
  wallet_cards.back().set_use_count(10U);

  // Setup the TestAutofillTable with the cards_on_disk.
  TestAutofillTable table(cards_on_disk);

  AutofillWalletSyncableService::CopyRelevantMetadataFromDisk(table,
                                                              &wallet_cards);

  ASSERT_EQ(1U, wallet_cards.size());

  // Make sure the use stats from disk were kept
  EXPECT_EQ(3U, wallet_cards.back().use_count());
  EXPECT_EQ(disk_time, wallet_cards.back().use_date());
}

// Verify that the use stats of a new Wallet card are as expected.
TEST(AutofillWalletSyncableServiceTest, NewWalletCard) {
  TestAutofillClock test_clock;
  base::Time arbitrary_time = base::Time::FromDoubleT(25);
  test_clock.SetNow(arbitrary_time);

  std::vector<AutofillProfile> wallet_addresses;
  std::vector<CreditCard> wallet_cards;
  syncer::SyncDataList data_list;

  // Create a Sync data for a card and its billing address.
  data_list.push_back(CreateSyncDataForWalletAddress("1" /* id */));
  data_list.push_back(CreateSyncDataForWalletCreditCard(
      "card1" /* id */, "1" /* billing_address_id */));

  AutofillWalletSyncableService::PopulateWalletCardsAndAddresses(
      data_list, &wallet_cards, &wallet_addresses);

  ASSERT_EQ(1U, wallet_cards.size());

  // The use_count should be 1 and the use_date should be the current time.
  EXPECT_EQ(1U, wallet_cards.back().use_count());
  EXPECT_EQ(arbitrary_time, wallet_cards.back().use_date());
}

}  // namespace autofill