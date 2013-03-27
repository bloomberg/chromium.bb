// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ui/autofill/autofill_dialog_models.h"
#include "chrome/browser/ui/autofill/data_model_wrapper.h"
#include "components/autofill/browser/credit_card.h"
#include "components/autofill/browser/field_types.h"
#include "components/autofill/browser/wallet/wallet_items.h"
#include "components/autofill/browser/wallet/wallet_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

TEST(AutofillCreditCardWrapperTest, GetInfoCreditCardExpMonth) {
  CreditCard card;
  MonthComboboxModel model;
  for (int month = 1; month <= 12; ++month) {
    card.SetRawInfo(CREDIT_CARD_EXP_MONTH, base::IntToString16(month));
    AutofillCreditCardWrapper wrapper(&card);
    EXPECT_EQ(model.GetItemAt(month), wrapper.GetInfo(CREDIT_CARD_EXP_MONTH));
  }
}

TEST(WalletInstrumentWrapperTest, GetInfoCreditCardExpMonth) {
  scoped_ptr<wallet::WalletItems::MaskedInstrument> instrument(
      wallet::GetTestMaskedInstrument());
  MonthComboboxModel model;
  for (int month = 1; month <= 12; ++month) {
    instrument->expiration_month_ = month;
    WalletInstrumentWrapper wrapper(instrument.get());
    EXPECT_EQ(model.GetItemAt(month), wrapper.GetInfo(CREDIT_CARD_EXP_MONTH));
  }
}

}  // autofill
