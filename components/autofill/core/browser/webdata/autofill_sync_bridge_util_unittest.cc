// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/webdata/autofill_sync_bridge_util.h"

#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/credit_card_cloud_token_data.h"
#include "components/autofill/core/browser/payments/payments_customer_data.h"
#include "components/autofill/core/browser/test_autofill_clock.h"
#include "components/autofill/core/browser/webdata/autofill_sync_bridge_test_util.h"
#include "components/autofill/core/browser/webdata/autofill_table.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/sync/base/client_tag_hash.h"
#include "components/sync/model/entity_data.h"
#include "components/sync/protocol/sync.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace {

using syncer::EntityChange;
using syncer::EntityData;

class TestAutofillTable : public AutofillTable {
 public:
  explicit TestAutofillTable(std::vector<CreditCard> cards_on_disk)
      : cards_on_disk_(cards_on_disk) {}

  ~TestAutofillTable() override {}

  bool GetServerCreditCards(
      std::vector<std::unique_ptr<CreditCard>>* cards) const override {
    for (const auto& card_on_disk : cards_on_disk_)
      cards->push_back(std::make_unique<CreditCard>(card_on_disk));
    return true;
  }

 private:
  std::vector<CreditCard> cards_on_disk_;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillTable);
};

EntityData SpecificsToEntity(const sync_pb::AutofillWalletSpecifics& specifics,
                             const std::string& client_tag) {
  syncer::EntityData data;
  *data.specifics.mutable_autofill_wallet() = specifics;
  data.client_tag_hash = syncer::ClientTagHash::FromUnhashed(
      syncer::AUTOFILL_WALLET_DATA, client_tag);
  return data;
}

class AutofillSyncBridgeUtilTest : public testing::Test {
 public:
  AutofillSyncBridgeUtilTest() {}
  ~AutofillSyncBridgeUtilTest() override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(AutofillSyncBridgeUtilTest);
};

// Tests that PopulateWalletTypesFromSyncData behaves as expected.
TEST_F(AutofillSyncBridgeUtilTest, PopulateWalletTypesFromSyncData) {
  // Add an address first.
  syncer::EntityChangeList entity_data;
  std::string address_id("address1");
  entity_data.push_back(EntityChange::CreateAdd(
      address_id,
      SpecificsToEntity(CreateAutofillWalletSpecificsForAddress(address_id),
                        /*client_tag=*/"address-address1")));
  // Add two credit cards.
  std::string credit_card_id_1 = "credit_card_1";
  std::string credit_card_id_2 = "credit_card_2";
  // Add the first card that has its billing address id set to the address's id.
  // No nickname is set.
  sync_pb::AutofillWalletSpecifics wallet_specifics_card1 =
      CreateAutofillWalletSpecificsForCard(
          /*id=*/credit_card_id_1,
          /*billing_address_id=*/address_id);
  // Add the second card that has nickname.
  std::string nickname("Grocery card");
  sync_pb::AutofillWalletSpecifics wallet_specifics_card2 =
      CreateAutofillWalletSpecificsForCard(
          /*id=*/credit_card_id_2,
          /*billing_address_id=*/"", /*nickname=*/nickname);
  // Set the second card's issuer to GOOGLE.
  wallet_specifics_card2.mutable_masked_card()
      ->mutable_card_issuer()
      ->set_issuer(sync_pb::CardIssuer::GOOGLE);
  entity_data.push_back(EntityChange::CreateAdd(
      credit_card_id_1,
      SpecificsToEntity(wallet_specifics_card1, /*client_tag=*/"card-card1")));
  entity_data.push_back(EntityChange::CreateAdd(
      credit_card_id_2,
      SpecificsToEntity(wallet_specifics_card2, /*client_tag=*/"card-card2")));
  // Add payments customer data.
  entity_data.push_back(EntityChange::CreateAdd(
      "deadbeef",
      SpecificsToEntity(CreateAutofillWalletSpecificsForPaymentsCustomerData(
                            /*specifics_id=*/"deadbeef"),
                        /*client_tag=*/"customer-deadbeef")));
  // Add cloud token data.
  entity_data.push_back(EntityChange::CreateAdd(
      "data1", SpecificsToEntity(
                   CreateAutofillWalletSpecificsForCreditCardCloudTokenData(
                       /*specifics_id=*/"data1"),
                   /*client_tag=*/"token-data1")));

  std::vector<CreditCard> wallet_cards;
  std::vector<AutofillProfile> wallet_addresses;
  std::vector<PaymentsCustomerData> customer_data;
  std::vector<CreditCardCloudTokenData> cloud_token_data;
  PopulateWalletTypesFromSyncData(entity_data, &wallet_cards, &wallet_addresses,
                                  &customer_data, &cloud_token_data);

  ASSERT_EQ(2U, wallet_cards.size());
  ASSERT_EQ(1U, wallet_addresses.size());

  EXPECT_EQ("deadbeef", customer_data.back().customer_id);

  EXPECT_EQ("data1", cloud_token_data.back().instrument_token);

  // Make sure the first card's billing address id is equal to the address'
  // server id.
  EXPECT_EQ(wallet_addresses.back().server_id(),
            wallet_cards.front().billing_address_id());
  // The first card's nickname is empty.
  EXPECT_TRUE(wallet_cards.front().nickname().empty());

  // Make sure the second card's nickname is correctly populated from sync data.
  EXPECT_EQ(base::UTF8ToUTF16(nickname), wallet_cards.back().nickname());

  // Verify that the card_issuer is set correctly.
  EXPECT_EQ(wallet_cards.front().card_issuer(), CreditCard::ISSUER_UNKNOWN);
  EXPECT_EQ(wallet_cards.back().card_issuer(), CreditCard::GOOGLE);
}

// Verify that the billing address id from the card saved on disk is kept if it
// is a local profile guid.
TEST_F(AutofillSyncBridgeUtilTest,
       CopyRelevantWalletMetadataFromDisk_KeepLocalAddresses) {
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

  CopyRelevantWalletMetadataFromDisk(table, &wallet_cards);

  ASSERT_EQ(1U, wallet_cards.size());

  // Make sure the wallet card replace its billing address id for the one that
  // was saved on disk.
  EXPECT_EQ(cards_on_disk.back().billing_address_id(),
            wallet_cards.back().billing_address_id());
}

// Verify that the billing address id from the card saved on disk is overwritten
// if it does not refer to a local profile.
TEST_F(AutofillSyncBridgeUtilTest,
       CopyRelevantWalletMetadataFromDisk_OverwriteOtherAddresses) {
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

  CopyRelevantWalletMetadataFromDisk(table, &wallet_cards);

  ASSERT_EQ(1U, wallet_cards.size());

  // Make sure the local address billing id that was saved on disk did not
  // replace the new one.
  EXPECT_EQ(new_billing_id, wallet_cards.back().billing_address_id());
}

// Verify that the use stats on disk are kept when server cards are synced.
TEST_F(AutofillSyncBridgeUtilTest,
       CopyRelevantWalletMetadataFromDisk_KeepUseStats) {
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

  CopyRelevantWalletMetadataFromDisk(table, &wallet_cards);

  ASSERT_EQ(1U, wallet_cards.size());

  // Make sure the use stats from disk were kept
  EXPECT_EQ(3U, wallet_cards.back().use_count());
  EXPECT_EQ(disk_time, wallet_cards.back().use_date());
}

}  // namespace
}  // namespace autofill
