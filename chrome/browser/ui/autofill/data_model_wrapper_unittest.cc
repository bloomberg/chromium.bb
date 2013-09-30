// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_dialog_models.h"
#include "chrome/browser/ui/autofill/data_model_wrapper.h"
#include "components/autofill/content/browser/wallet/wallet_items.h"
#include "components/autofill/content/browser/wallet/wallet_test_util.h"
#include "components/autofill/core/browser/autofill_common_test.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

TEST(AutofillCreditCardWrapperTest, GetInfoCreditCardExpMonth) {
  CreditCard card;
  MonthComboboxModel model;
  for (int month = 1; month <= 12; ++month) {
    card.SetRawInfo(CREDIT_CARD_EXP_MONTH, base::IntToString16(month));
    AutofillCreditCardWrapper wrapper(&card);
    EXPECT_EQ(model.GetItemAt(month),
              wrapper.GetInfo(AutofillType(CREDIT_CARD_EXP_MONTH)));
  }
}

TEST(AutofillCreditCardWrapperTest, GetDisplayTextEmptyWhenExpired) {
  CreditCard card;
  card.SetRawInfo(CREDIT_CARD_EXP_MONTH, ASCIIToUTF16("1"));
  card.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, ASCIIToUTF16("2010"));
  card.SetRawInfo(CREDIT_CARD_NUMBER, ASCIIToUTF16("4111111111111111"));
  AutofillCreditCardWrapper wrapper(&card);
  base::string16 unused, unused2;
  EXPECT_FALSE(wrapper.GetDisplayText(&unused, &unused2));
}

TEST(AutofillCreditCardWrapperTest, GetDisplayTextEmptyWhenInvalid) {
  CreditCard card;
  card.SetRawInfo(CREDIT_CARD_EXP_MONTH, ASCIIToUTF16("12"));
  card.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, ASCIIToUTF16("9999"));
  card.SetRawInfo(CREDIT_CARD_NUMBER, ASCIIToUTF16("41111"));
  AutofillCreditCardWrapper wrapper(&card);
  base::string16 unused, unused2;
  EXPECT_FALSE(wrapper.GetDisplayText(&unused, &unused2));
}

TEST(AutofillCreditCardWrapperTest, GetDisplayTextNotEmptyWhenValid) {
  CreditCard card;
  card.SetRawInfo(CREDIT_CARD_EXP_MONTH, ASCIIToUTF16("12"));
  card.SetRawInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, ASCIIToUTF16("9999"));
  card.SetRawInfo(CREDIT_CARD_NUMBER, ASCIIToUTF16("4111111111111111"));
  AutofillCreditCardWrapper wrapper(&card);
  base::string16 unused, unused2;
  EXPECT_TRUE(wrapper.GetDisplayText(&unused, &unused2));
}

TEST(WalletInstrumentWrapperTest, GetInfoCreditCardExpMonth) {
  scoped_ptr<wallet::WalletItems::MaskedInstrument> instrument(
      wallet::GetTestMaskedInstrument());
  MonthComboboxModel model;
  for (int month = 1; month <= 12; ++month) {
    instrument->expiration_month_ = month;
    WalletInstrumentWrapper wrapper(instrument.get());
    EXPECT_EQ(model.GetItemAt(month),
              wrapper.GetInfo(AutofillType(CREDIT_CARD_EXP_MONTH)));
  }
}

TEST(WalletInstrumentWrapperTest, GetDisplayTextEmptyWhenExpired) {
  scoped_ptr<wallet::WalletItems::MaskedInstrument> instrument(
      wallet::GetTestMaskedInstrument());
  instrument->status_ = wallet::WalletItems::MaskedInstrument::EXPIRED;
  WalletInstrumentWrapper wrapper(instrument.get());
  base::string16 unused, unused2;
  EXPECT_FALSE(wrapper.GetDisplayText(&unused, &unused2));
}

TEST(DataModelWrapperTest, GetDisplayTextEmptyWithoutPhone) {
  scoped_ptr<wallet::WalletItems::MaskedInstrument> instrument(
      wallet::GetTestMaskedInstrument());

  WalletInstrumentWrapper instrument_wrapper(instrument.get());
  base::string16 unused, unused2;
  ASSERT_TRUE(instrument_wrapper.GetDisplayText(&unused, &unused2));

  WalletAddressWrapper address_wrapper(&instrument->address());
  ASSERT_TRUE(address_wrapper.GetDisplayText(&unused, &unused2));

  const_cast<wallet::Address*>(&instrument->address())->set_phone_number(
      string16());

  ASSERT_TRUE(
      instrument_wrapper.GetInfo(
          AutofillType(PHONE_HOME_WHOLE_NUMBER)).empty());
  EXPECT_FALSE(instrument_wrapper.GetDisplayText(&unused, &unused2));

  ASSERT_TRUE(
      address_wrapper.GetInfo(AutofillType(PHONE_HOME_WHOLE_NUMBER)).empty());
  EXPECT_FALSE(address_wrapper.GetDisplayText(&unused, &unused2));
}

}  // namespace autofill
