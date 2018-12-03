// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/wallet_helper.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/sync/test/fake_server/fake_server_http_post_provider.h"
#include "net/base/network_change_notifier.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using autofill::AutofillProfile;
using autofill::CreditCard;
using autofill::PersonalDataManager;
using wallet_helper::CreateDefaultSyncPaymentsCustomerData;
using wallet_helper::CreateSyncWalletAddress;
using wallet_helper::CreateSyncWalletCard;
using wallet_helper::GetCreditCard;
using wallet_helper::GetLocalProfiles;
using wallet_helper::GetPersonalDataManager;
using wallet_helper::GetServerCreditCards;
using wallet_helper::GetServerProfiles;
using wallet_helper::kDefaultBillingAddressID;
using wallet_helper::UpdateServerAddressMetadata;
using wallet_helper::UpdateServerCardMetadata;

const char kDifferentBillingAddressId[] = "another address entity ID";
constexpr char kLocalBillingAddressId[] =
    "local billing address ID has size 36";
constexpr char kLocalBillingAddressId2[] =
    "another local billing address id wow";
static_assert(sizeof(kLocalBillingAddressId) == autofill::kLocalGuidSize + 1,
              "|kLocalBillingAddressId| has to have the right length to be "
              "considered a local guid");
static_assert(sizeof(kLocalBillingAddressId) == sizeof(kLocalBillingAddressId2),
              "|kLocalBillingAddressId2| has to have the right length to be "
              "considered a local guid");

class TwoClientWalletSyncTest : public UssWalletSwitchToggler, public SyncTest {
 public:
  TwoClientWalletSyncTest() : SyncTest(TWO_CLIENT) {}
  ~TwoClientWalletSyncTest() override {}

  bool TestUsesSelfNotifications() override { return false; }

  bool SetupSync() override {
    if (!SyncTest::SetupSync()) {
      return false;
    }

    // Plug in SyncService into PDM so that it can check we use full sync.
    GetPersonalDataManager(0)->OnSyncServiceInitialized(GetSyncService(0));
    GetPersonalDataManager(1)->OnSyncServiceInitialized(GetSyncService(1));

    // As this test does not use self notifications, wait for the metadata to
    // converge with the specialized wallet checker.
    return AutofillWalletChecker(0, 1).Wait();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(TwoClientWalletSyncTest);
};

IN_PROC_BROWSER_TEST_P(TwoClientWalletSyncTest, UpdateCreditCardMetadata) {
  InitWithDefaultFeatures();

  GetFakeServer()->SetWalletData(
      {CreateSyncWalletCard(/*name=*/"card-1", /*last_four=*/"0001",
                            kDefaultBillingAddressID),
       CreateSyncWalletAddress(/*name=*/"address-1", /*company=*/"Company-1"),
       CreateDefaultSyncPaymentsCustomerData()});
  ASSERT_TRUE(SetupSync());

  // Grab the current card on the first client.
  std::vector<CreditCard*> credit_cards = GetServerCreditCards(0);
  ASSERT_EQ(1u, credit_cards.size());
  CreditCard card = *credit_cards[0];

  // Simulate using it -- increase both its use count and use date.
  ASSERT_EQ(1u, card.use_count());
  card.set_use_count(2);
  base::Time new_use_date = base::Time::Now();
  ASSERT_NE(new_use_date, card.use_date());
  card.set_use_date(new_use_date);
  UpdateServerCardMetadata(0, card);

  // Wait for the change to propagate.
  EXPECT_TRUE(AutofillWalletChecker(0, 1).Wait());

  credit_cards = GetServerCreditCards(1);
  EXPECT_EQ(1U, credit_cards.size());
  EXPECT_EQ(2u, credit_cards[0]->use_count());
  EXPECT_EQ(new_use_date, credit_cards[0]->use_date());

  credit_cards = GetServerCreditCards(0);
  EXPECT_EQ(1U, credit_cards.size());
  EXPECT_EQ(2u, credit_cards[0]->use_count());
  EXPECT_EQ(new_use_date, credit_cards[0]->use_date());
}

IN_PROC_BROWSER_TEST_P(TwoClientWalletSyncTest,
                       UpdateCreditCardMetadataWhileNotSyncing) {
  InitWithDefaultFeatures();

  GetFakeServer()->SetWalletData(
      {CreateSyncWalletCard(/*name=*/"card-1", /*last_four=*/"0001",
                            kDefaultBillingAddressID),
       CreateDefaultSyncPaymentsCustomerData()});
  ASSERT_TRUE(SetupSync());

  // Sumulate going offline on both clients.
  fake_server::FakeServerHttpPostProvider::DisableNetwork();

  // Grab the current card on the first client.
  std::vector<CreditCard*> credit_cards = GetServerCreditCards(0);
  ASSERT_EQ(1u, credit_cards.size());
  CreditCard card = *credit_cards[0];

  // Simulate using it -- increase both its use count and use date.
  ASSERT_EQ(1u, card.use_count());
  card.set_use_count(2);
  base::Time new_use_date = base::Time::Now();
  ASSERT_NE(new_use_date, card.use_date());
  card.set_use_date(new_use_date);
  UpdateServerCardMetadata(0, card);

  // Simulate going online again.
  fake_server::FakeServerHttpPostProvider::EnableNetwork();
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_ETHERNET);

  // Wait for the change to propagate.
  EXPECT_TRUE(AutofillWalletChecker(0, 1).Wait());

  credit_cards = GetServerCreditCards(0);
  EXPECT_EQ(1U, credit_cards.size());
  EXPECT_EQ(2u, credit_cards[0]->use_count());
  EXPECT_EQ(new_use_date, credit_cards[0]->use_date());

  credit_cards = GetServerCreditCards(1);
  EXPECT_EQ(1U, credit_cards.size());
  EXPECT_EQ(2u, credit_cards[0]->use_count());
  EXPECT_EQ(new_use_date, credit_cards[0]->use_date());
}

IN_PROC_BROWSER_TEST_P(TwoClientWalletSyncTest,
                       UpdateCreditCardMetadataConflictsWhileNotSyncing) {
  InitWithDefaultFeatures();

  GetFakeServer()->SetWalletData(
      {CreateSyncWalletCard(/*name=*/"card-1", /*last_four=*/"0001",
                            kDefaultBillingAddressID),
       CreateDefaultSyncPaymentsCustomerData()});
  ASSERT_TRUE(SetupSync());

  // Sumulate going offline on both clients.
  fake_server::FakeServerHttpPostProvider::DisableNetwork();

  // Increase use stats on both clients, make use count higher on the first
  // client and use date higher on the second client.
  std::vector<CreditCard*> credit_cards = GetServerCreditCards(0);
  ASSERT_EQ(1u, credit_cards.size());
  CreditCard card = *credit_cards[0];
  ASSERT_EQ(1u, card.use_count());
  card.set_use_count(3);
  base::Time lower_new_use_date = base::Time::Now();
  ASSERT_NE(lower_new_use_date, card.use_date());
  card.set_use_date(lower_new_use_date);
  UpdateServerCardMetadata(0, card);

  credit_cards = GetServerCreditCards(1);
  ASSERT_EQ(1u, credit_cards.size());
  card = *credit_cards[0];
  ASSERT_EQ(1u, card.use_count());
  card.set_use_count(2);
  base::Time higher_new_use_date = base::Time::Now();
  ASSERT_NE(higher_new_use_date, card.use_date());
  ASSERT_LT(lower_new_use_date, higher_new_use_date);
  card.set_use_date(higher_new_use_date);
  UpdateServerCardMetadata(1, card);

  // Simulate going online again.
  fake_server::FakeServerHttpPostProvider::EnableNetwork();
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_ETHERNET);

  // Wait for the clients to coverge and both resolve the conflicts by taking
  // maxima in both components.
  EXPECT_TRUE(AutofillWalletChecker(0, 1).Wait());

  credit_cards = GetServerCreditCards(0);
  EXPECT_EQ(1U, credit_cards.size());
  EXPECT_EQ(3u, credit_cards[0]->use_count());
  EXPECT_EQ(higher_new_use_date, credit_cards[0]->use_date());
  credit_cards = GetServerCreditCards(1);
  EXPECT_EQ(1U, credit_cards.size());
  EXPECT_EQ(3u, credit_cards[0]->use_count());
  EXPECT_EQ(higher_new_use_date, credit_cards[0]->use_date());
}

IN_PROC_BROWSER_TEST_P(TwoClientWalletSyncTest, UpdateServerAddressMetadata) {
  InitWithDefaultFeatures();

  GetFakeServer()->SetWalletData(
      {CreateSyncWalletCard(/*name=*/"card-1", /*last_four=*/"0001",
                            kDefaultBillingAddressID),
       CreateSyncWalletAddress(/*name=*/"address-1", /*company=*/"Company-1"),
       CreateDefaultSyncPaymentsCustomerData()});
  ASSERT_TRUE(SetupSync());

  // Grab the current address on the first client.
  std::vector<AutofillProfile*> server_addresses = GetServerProfiles(0);
  ASSERT_EQ(1u, server_addresses.size());
  AutofillProfile address = *server_addresses[0];

  // Simulate using it -- increase both its use count and use date.
  ASSERT_EQ(1u, address.use_count());
  address.set_use_count(2);
  base::Time new_use_date = base::Time::Now();
  ASSERT_NE(new_use_date, address.use_date());
  address.set_use_date(new_use_date);
  UpdateServerAddressMetadata(0, address);

  // Wait for the change to propagate.
  EXPECT_TRUE(AutofillWalletChecker(0, 1).Wait());

  server_addresses = GetServerProfiles(1);
  EXPECT_EQ(1U, server_addresses.size());
  EXPECT_EQ(2u, server_addresses[0]->use_count());
  EXPECT_EQ(new_use_date, server_addresses[0]->use_date());

  server_addresses = GetServerProfiles(0);
  EXPECT_EQ(1U, server_addresses.size());
  EXPECT_EQ(2u, server_addresses[0]->use_count());
  EXPECT_EQ(new_use_date, server_addresses[0]->use_date());
}

IN_PROC_BROWSER_TEST_P(TwoClientWalletSyncTest,
                       UpdateServerAddressMetadataWhileNotSyncing) {
  InitWithDefaultFeatures();

  GetFakeServer()->SetWalletData(
      {CreateSyncWalletAddress(/*name=*/"address-1", /*company=*/"Company-1"),
       CreateDefaultSyncPaymentsCustomerData()});
  ASSERT_TRUE(SetupSync());

  // Sumulate going offline on both clients.
  fake_server::FakeServerHttpPostProvider::DisableNetwork();

  // Grab the current address on the first client.
  std::vector<AutofillProfile*> server_addresses = GetServerProfiles(0);
  ASSERT_EQ(1u, server_addresses.size());
  AutofillProfile address = *server_addresses[0];

  // Simulate using it -- increase both its use count and use date.
  ASSERT_EQ(1u, address.use_count());
  address.set_use_count(2);
  base::Time new_use_date = base::Time::Now();
  ASSERT_NE(new_use_date, address.use_date());
  address.set_use_date(new_use_date);
  UpdateServerAddressMetadata(0, address);

  // Simulate going online again.
  fake_server::FakeServerHttpPostProvider::EnableNetwork();
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_ETHERNET);

  // Wait for the change to propagate.
  EXPECT_TRUE(AutofillWalletChecker(0, 1).Wait());

  server_addresses = GetServerProfiles(1);
  EXPECT_EQ(1U, server_addresses.size());
  EXPECT_EQ(2u, server_addresses[0]->use_count());
  EXPECT_EQ(new_use_date, server_addresses[0]->use_date());

  server_addresses = GetServerProfiles(0);
  EXPECT_EQ(1U, server_addresses.size());
  EXPECT_EQ(2u, server_addresses[0]->use_count());
  EXPECT_EQ(new_use_date, server_addresses[0]->use_date());
}

IN_PROC_BROWSER_TEST_P(TwoClientWalletSyncTest,
                       UpdateServerAddressMetadataConflictsWhileNotSyncing) {
  InitWithDefaultFeatures();

  GetFakeServer()->SetWalletData(
      {CreateSyncWalletAddress(/*name=*/"address-1", /*company=*/"Company-1"),
       CreateDefaultSyncPaymentsCustomerData()});
  ASSERT_TRUE(SetupSync());

  // Sumulate going offline on both clients.
  fake_server::FakeServerHttpPostProvider::DisableNetwork();

  // Increase use stats on both clients, make use count higher on the first
  // client and use date higher on the second client.
  std::vector<AutofillProfile*> server_addresses = GetServerProfiles(0);
  ASSERT_EQ(1u, server_addresses.size());
  AutofillProfile address = *server_addresses[0];
  ASSERT_EQ(1u, address.use_count());
  address.set_use_count(3);
  base::Time lower_new_use_date = base::Time::Now();
  ASSERT_NE(lower_new_use_date, address.use_date());
  address.set_use_date(lower_new_use_date);
  UpdateServerAddressMetadata(0, address);

  server_addresses = GetServerProfiles(1);
  ASSERT_EQ(1u, server_addresses.size());
  address = *server_addresses[0];
  ASSERT_EQ(1u, address.use_count());
  address.set_use_count(2);
  base::Time higher_new_use_date = base::Time::Now();
  ASSERT_NE(higher_new_use_date, address.use_date());
  ASSERT_LT(lower_new_use_date, higher_new_use_date);
  address.set_use_date(higher_new_use_date);
  UpdateServerAddressMetadata(1, address);

  // Simulate going online again.
  fake_server::FakeServerHttpPostProvider::EnableNetwork();
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_ETHERNET);

  // Wait for the clients to coverge and both resolve the conflicts by taking
  // maxima in both components.
  EXPECT_TRUE(AutofillWalletChecker(0, 1).Wait());

  server_addresses = GetServerProfiles(0);
  EXPECT_EQ(1U, server_addresses.size());
  EXPECT_EQ(3u, server_addresses[0]->use_count());
  EXPECT_EQ(higher_new_use_date, server_addresses[0]->use_date());
  server_addresses = GetServerProfiles(1);
  EXPECT_EQ(1U, server_addresses.size());
  EXPECT_EQ(3u, server_addresses[0]->use_count());
  EXPECT_EQ(higher_new_use_date, server_addresses[0]->use_date());
}

IN_PROC_BROWSER_TEST_P(TwoClientWalletSyncTest,
                       UpdateCreditCardMetadataWithNewBillingAddressId) {
  InitWithDefaultFeatures();

  GetFakeServer()->SetWalletData(
      {CreateSyncWalletCard(/*name=*/"card-1", /*last_four=*/"0001",
                            /*billing_address_id=*/""),
       CreateDefaultSyncPaymentsCustomerData()});
  ASSERT_TRUE(SetupSync());

  // Grab the current card on the first client.
  std::vector<CreditCard*> credit_cards = GetServerCreditCards(0);
  ASSERT_EQ(1U, credit_cards.size());
  CreditCard card = *credit_cards[0];
  ASSERT_TRUE(card.billing_address_id().empty());

  // Update the billing address.
  card.set_billing_address_id(kDefaultBillingAddressID);
  UpdateServerCardMetadata(0, card);
  EXPECT_TRUE(AutofillWalletChecker(0, 1).Wait());

  // Make sure both clients have the updated billing_address_id.
  credit_cards = GetServerCreditCards(1);
  EXPECT_EQ(1U, credit_cards.size());
  EXPECT_EQ(kDefaultBillingAddressID, credit_cards[0]->billing_address_id());

  credit_cards = GetServerCreditCards(0);
  EXPECT_EQ(1U, credit_cards.size());
  EXPECT_EQ(kDefaultBillingAddressID, credit_cards[0]->billing_address_id());
}

IN_PROC_BROWSER_TEST_P(TwoClientWalletSyncTest,
                       UpdateCreditCardMetadataWithChangedBillingAddressId) {
  InitWithDefaultFeatures();

  GetFakeServer()->SetWalletData(
      {CreateSyncWalletCard(/*name=*/"card-1", /*last_four=*/"0001",
                            kDefaultBillingAddressID),
       CreateDefaultSyncPaymentsCustomerData()});
  ASSERT_TRUE(SetupSync());

  // Grab the current card on the first client.
  std::vector<CreditCard*> credit_cards = GetServerCreditCards(0);
  ASSERT_EQ(1U, credit_cards.size());
  CreditCard card = *credit_cards[0];

  // Update the billing address.
  ASSERT_EQ(kDefaultBillingAddressID, card.billing_address_id());
  card.set_billing_address_id(kDifferentBillingAddressId);
  UpdateServerCardMetadata(0, card);
  EXPECT_TRUE(AutofillWalletChecker(0, 1).Wait());

  // Make sure both clients have the updated billing_address_id.
  credit_cards = GetServerCreditCards(1);
  EXPECT_EQ(1U, credit_cards.size());
  EXPECT_EQ(kDifferentBillingAddressId, credit_cards[0]->billing_address_id());

  credit_cards = GetServerCreditCards(0);
  EXPECT_EQ(1U, credit_cards.size());
  EXPECT_EQ(kDifferentBillingAddressId, credit_cards[0]->billing_address_id());
}

IN_PROC_BROWSER_TEST_P(
    TwoClientWalletSyncTest,
    UpdateCreditCardMetadataWithChangedBillingAddressId_RemoteToLocal) {
  InitWithDefaultFeatures();

  GetFakeServer()->SetWalletData(
      {CreateSyncWalletCard(/*name=*/"card-1", /*last_four=*/"0001",
                            kDefaultBillingAddressID),
       CreateDefaultSyncPaymentsCustomerData()});
  ASSERT_TRUE(SetupSync());

  // Grab the current card on the first client.
  std::vector<CreditCard*> credit_cards = GetServerCreditCards(0);
  ASSERT_EQ(1U, credit_cards.size());
  CreditCard card = *credit_cards[0];
  ASSERT_EQ(kDefaultBillingAddressID, card.billing_address_id());

  // Update the billing address (replace a remote profile by a local profile).
  card.set_billing_address_id(kLocalBillingAddressId);
  UpdateServerCardMetadata(0, card);
  EXPECT_TRUE(AutofillWalletChecker(0, 1).Wait());

  // Make sure both clients have the updated billing_address_id (local profile
  // wins).
  credit_cards = GetServerCreditCards(1);
  EXPECT_EQ(1U, credit_cards.size());
  EXPECT_EQ(kLocalBillingAddressId, credit_cards[0]->billing_address_id());

  credit_cards = GetServerCreditCards(0);
  EXPECT_EQ(1U, credit_cards.size());
  EXPECT_EQ(kLocalBillingAddressId, credit_cards[0]->billing_address_id());
}

IN_PROC_BROWSER_TEST_P(
    TwoClientWalletSyncTest,
    UpdateCreditCardMetadataWithChangedBillingAddressId_LocalToRemote) {
  InitWithDefaultFeatures();

  GetFakeServer()->SetWalletData(
      {CreateSyncWalletCard(/*name=*/"card-1", /*last_four=*/"0001",
                            kLocalBillingAddressId),
       CreateDefaultSyncPaymentsCustomerData()});
  ASSERT_TRUE(SetupSync());

  // Grab the current card on the first client.
  std::vector<CreditCard*> credit_cards = GetServerCreditCards(0);
  ASSERT_EQ(1U, credit_cards.size());
  CreditCard card = *credit_cards[0];

  // Update the billing address (replace a local profile by a remote profile).
  ASSERT_EQ(kLocalBillingAddressId, card.billing_address_id());
  card.set_billing_address_id(kDifferentBillingAddressId);
  UpdateServerCardMetadata(0, card);
  EXPECT_TRUE(AutofillWalletChecker(0, 1).Wait());

  // Make sure both clients have the original billing_address_id (local profile
  // wins).
  credit_cards = GetServerCreditCards(1);
  EXPECT_EQ(1U, credit_cards.size());
  EXPECT_EQ(kLocalBillingAddressId, credit_cards[0]->billing_address_id());

  credit_cards = GetServerCreditCards(0);
  EXPECT_EQ(1U, credit_cards.size());
  EXPECT_EQ(kLocalBillingAddressId, credit_cards[0]->billing_address_id());
}

IN_PROC_BROWSER_TEST_P(
    TwoClientWalletSyncTest,
    UpdateCreditCardMetadataWithChangedBillingAddressId_LocalToRemoteOffline) {
  InitWithDefaultFeatures();

  GetFakeServer()->SetWalletData(
      {CreateSyncWalletCard(/*name=*/"card-1", /*last_four=*/"0001",
                            kLocalBillingAddressId),
       CreateDefaultSyncPaymentsCustomerData()});
  ASSERT_TRUE(SetupSync());

  // Sumulate going offline on both clients.
  fake_server::FakeServerHttpPostProvider::DisableNetwork();

  // Grab the current card on the first client.
  std::vector<CreditCard*> credit_cards = GetServerCreditCards(0);
  ASSERT_EQ(1U, credit_cards.size());
  CreditCard card = *credit_cards[0];

  // Update the billing address (replace a local profile by a remote profile).
  ASSERT_EQ(kLocalBillingAddressId, card.billing_address_id());
  card.set_billing_address_id(kDifferentBillingAddressId);
  UpdateServerCardMetadata(0, card);

  // Simulate going online again.
  fake_server::FakeServerHttpPostProvider::EnableNetwork();
  net::NetworkChangeNotifier::NotifyObserversOfNetworkChangeForTests(
      net::NetworkChangeNotifier::CONNECTION_ETHERNET);

  EXPECT_TRUE(AutofillWalletChecker(0, 1).Wait());

  // Make sure both clients have the original billing_address_id (local profile
  // wins).
  credit_cards = GetServerCreditCards(1);
  EXPECT_EQ(1U, credit_cards.size());
  EXPECT_EQ(kLocalBillingAddressId, credit_cards[0]->billing_address_id());

  credit_cards = GetServerCreditCards(0);
  EXPECT_EQ(1U, credit_cards.size());
  EXPECT_EQ(kLocalBillingAddressId, credit_cards[0]->billing_address_id());
}

IN_PROC_BROWSER_TEST_P(TwoClientWalletSyncTest,
                       ServerAddressConvertsToSameLocalAddress) {
  InitWithDefaultFeatures();

  GetFakeServer()->SetWalletData(
      {CreateSyncWalletAddress(/*name=*/"address-1", /*company=*/"Company-1"),
       CreateDefaultSyncPaymentsCustomerData()});
  ASSERT_TRUE(SetupSync());

  // Make sure both have has_converted true.
  std::vector<AutofillProfile*> server_addresses = GetServerProfiles(0);
  EXPECT_EQ(1u, server_addresses.size());
  EXPECT_TRUE(server_addresses[0]->has_converted());

  server_addresses = GetServerProfiles(1);
  EXPECT_EQ(1U, server_addresses.size());
  EXPECT_TRUE(server_addresses[0]->has_converted());

  // Make sure they have the same local profile.
  std::vector<AutofillProfile*> local_addresses = GetLocalProfiles(0);
  EXPECT_EQ(1u, local_addresses.size());
  const std::string& guid = local_addresses[0]->guid();

  local_addresses = GetLocalProfiles(1);
  EXPECT_EQ(1u, local_addresses.size());
  EXPECT_EQ(guid, local_addresses[0]->guid());
}

INSTANTIATE_TEST_CASE_P(USS,
                        TwoClientWalletSyncTest,
                        ::testing::Values(false, true));

}  // namespace
