// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/macros.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/sync/test/integration/wallet_helper.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

using autofill::AutofillProfile;
using autofill::CreditCard;
using autofill::PersonalDataManager;
using wallet_helper::CreateDefaultSyncPaymentsCustomerData;
using wallet_helper::CreateSyncWalletAddress;
using wallet_helper::CreateSyncWalletCard;
using wallet_helper::GetCreditCard;
using wallet_helper::GetServerCreditCards;
using wallet_helper::GetServerProfiles;
using wallet_helper::UpdateServerAddressMetadata;
using wallet_helper::UpdateServerCardMetadata;

class TwoClientWalletSyncTest : public UssWalletSwitchToggler, public SyncTest {
 public:
  TwoClientWalletSyncTest() : SyncTest(TWO_CLIENT) {}
  ~TwoClientWalletSyncTest() override {}

  bool TestUsesSelfNotifications() override { return false; }

  bool SetupSync() override {
    if (!SyncTest::SetupSync()) {
      return false;
    }
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
      {CreateSyncWalletCard(/*name=*/"card-1", /*last_four=*/"0001"),
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

INSTANTIATE_TEST_CASE_P(USS,
                        TwoClientWalletSyncTest,
                        ::testing::Values(false, true));

}  // namespace
