// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/autofill/content/browser/wallet/instrument.h"
#include "components/autofill/content/browser/wallet/wallet_address.h"
#include "components/autofill/content/browser/wallet/wallet_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace {

const char kPrimaryAccountNumber[] = "4444444444444448";
const char kCardVerificationNumber[] = "123";
const char kLastFourDigits[] = "4448";

}

namespace autofill {
namespace wallet {

TEST(Instrument, LastFourDigits) {
  Instrument instrument(ASCIIToUTF16(kPrimaryAccountNumber),
                        ASCIIToUTF16(kCardVerificationNumber),
                        12,
                        2015,
                        Instrument::VISA,
                        GetTestShippingAddress().Pass());

  EXPECT_EQ(ASCIIToUTF16(kLastFourDigits), instrument.last_four_digits());
}

TEST(Instrument, ToDictionary) {
  base::DictionaryValue expected;
  expected.SetString("type", "CREDIT_CARD");
  expected.SetInteger("credit_card.exp_month", 12);
  expected.SetInteger("credit_card.exp_year", 2015);
  expected.SetString("credit_card.last_4_digits", kLastFourDigits);
  expected.SetString("credit_card.fop_type", "VISA");
  expected.SetString("credit_card.address.country_name_code", "US");
  expected.SetString("credit_card.address.recipient_name",
                     "ship_recipient_name");
  expected.SetString("credit_card.address.locality_name",
                     "ship_locality_name");
  expected.SetString("credit_card.address.dependent_locality_name",
                     "ship_dependent_locality_name");
  expected.SetString("credit_card.address.administrative_area_name",
                     "ship_admin_area_name");
  expected.SetString("credit_card.address.postal_code_number",
                     "ship_postal_code_number");
  expected.SetString("credit_card.address.sorting_code",
                     "ship_sorting_code");
  base::ListValue* address_lines = new base::ListValue();
  address_lines->AppendString("ship_address_line_1");
  address_lines->AppendString("ship_address_line_2");
  expected.Set("credit_card.address.address_line", address_lines);
  expected.SetString("credit_card.address.language_code", "ship_language_code");

  Instrument instrument(ASCIIToUTF16(kPrimaryAccountNumber),
                        ASCIIToUTF16(kCardVerificationNumber),
                        12,
                        2015,
                        Instrument::VISA,
                        GetTestShippingAddress().Pass());

  EXPECT_TRUE(expected.Equals(instrument.ToDictionary().get()));
}

}  // namespace wallet
}  // namespace autofill
