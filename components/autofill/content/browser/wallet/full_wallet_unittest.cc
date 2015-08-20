// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/json/json_reader.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/autofill/content/browser/wallet/full_wallet.h"
#include "components/autofill/content/browser/wallet/wallet_test_util.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace autofill {
namespace wallet {

class FullWalletTest : public testing::Test {
 public:
  FullWalletTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(FullWalletTest);
};

TEST_F(FullWalletTest, RestLengthCorrectDecryptionTest) {
  FullWallet full_wallet(12, 2012, "528512", "5ec4feecf9d6", GetTestAddress(),
                         GetTestShippingAddress());
  std::vector<uint8> one_time_pad;
  EXPECT_TRUE(base::HexStringToBytes("5F04A8704183", &one_time_pad));
  full_wallet.set_one_time_pad(one_time_pad);
  EXPECT_EQ(ASCIIToUTF16("5285121925598459"),
            full_wallet.GetInfo("", AutofillType(CREDIT_CARD_NUMBER)));
  EXPECT_EQ(ASCIIToUTF16("989"),
            full_wallet.GetInfo(
                "", AutofillType(CREDIT_CARD_VERIFICATION_CODE)));
}

TEST_F(FullWalletTest, RestLengthUnderDecryptionTest) {
  FullWallet full_wallet(12, 2012, "528512", "4c567667e6", GetTestAddress(),
                         GetTestShippingAddress());
  std::vector<uint8> one_time_pad;
  EXPECT_TRUE(base::HexStringToBytes("063AD35324BF", &one_time_pad));
  full_wallet.set_one_time_pad(one_time_pad);
  EXPECT_EQ(ASCIIToUTF16("5285127106109719"),
            full_wallet.GetInfo("", AutofillType(CREDIT_CARD_NUMBER)));
  EXPECT_EQ(ASCIIToUTF16("385"),
            full_wallet.GetInfo(
                "", AutofillType(CREDIT_CARD_VERIFICATION_CODE)));
}

TEST_F(FullWalletTest, GetCreditCardInfo) {
  FullWallet full_wallet(12, 2015, "528512", "1a068673eb0", GetTestAddress(),
                         GetTestShippingAddress());

  EXPECT_EQ(ASCIIToUTF16("15"),
            full_wallet.GetInfo(
                "", AutofillType(CREDIT_CARD_EXP_2_DIGIT_YEAR)));

  EXPECT_EQ(ASCIIToUTF16("12/15"),
            full_wallet.GetInfo(
                "", AutofillType(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR)));

  EXPECT_EQ(ASCIIToUTF16("12/2015"),
            full_wallet.GetInfo(
                "", AutofillType(CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR)));

  std::vector<uint8> one_time_pad;
  EXPECT_TRUE(base::HexStringToBytes("075DA779F98B", &one_time_pad));
  full_wallet.set_one_time_pad(one_time_pad);
  EXPECT_EQ(ASCIIToUTF16("MasterCard"),
            full_wallet.GetInfo("", AutofillType(CREDIT_CARD_TYPE)));
}

TEST_F(FullWalletTest, CreateFullWalletFromClearTextData) {
  scoped_ptr<FullWallet> full_wallet =
      FullWallet::CreateFullWalletFromClearText(
          11, 2012,
          "5555555555554444", "123",
          GetTestAddress(), GetTestShippingAddress());
  EXPECT_EQ(ASCIIToUTF16("5555555555554444"),
            full_wallet->GetInfo("", AutofillType(CREDIT_CARD_NUMBER)));
  EXPECT_EQ(ASCIIToUTF16("MasterCard"),
            full_wallet->GetInfo("", AutofillType(CREDIT_CARD_TYPE)));
  EXPECT_EQ(ASCIIToUTF16("123"),
            full_wallet->GetInfo(
                "", AutofillType(CREDIT_CARD_VERIFICATION_CODE)));
  EXPECT_EQ(ASCIIToUTF16("11/12"),
            full_wallet->GetInfo(
                "", AutofillType(CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR)));
  EXPECT_TRUE(GetTestAddress()->EqualsIgnoreID(
      *full_wallet->billing_address()));
  EXPECT_TRUE(GetTestShippingAddress()->EqualsIgnoreID(
      *full_wallet->shipping_address()));
}

}  // namespace wallet
}  // namespace autofill
