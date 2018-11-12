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

  // Increase use count on the first client.
  CreditCard card = GetCreditCard(/*name=*/"card-1", /*last_four=*/"0001");
  card.set_use_count(5);
  UpdateServerCardMetadata(0, card);
  EXPECT_TRUE(AutofillWalletChecker(0, 1).Wait());

  // Make sure both clients have use_count=5.
  std::vector<CreditCard*> credit_cards = GetServerCreditCards(1);
  EXPECT_EQ(1U, credit_cards.size());
  EXPECT_EQ(5u, credit_cards[0]->use_count());

  credit_cards = GetServerCreditCards(0);
  EXPECT_EQ(1U, credit_cards.size());
  EXPECT_EQ(5u, credit_cards[0]->use_count());
}

INSTANTIATE_TEST_CASE_P(USS,
                        TwoClientWalletSyncTest,
                        ::testing::Values(false, true));

}  // namespace
