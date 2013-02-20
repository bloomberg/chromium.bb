// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/autofill/wallet/instrument.h"
#include "chrome/browser/autofill/wallet/wallet_address.h"
#include "chrome/browser/autofill/wallet/wallet_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kPrimaryAccountNumber[] = "4444444444444448";
const char kCardVerificationNumber[] = "123";
const char kLastFourDigits[] = "4448";

}

namespace wallet {

TEST(Instrument, LastFourDigits) {
  Instrument instrument(ASCIIToUTF16(kPrimaryAccountNumber),
                        ASCIIToUTF16(kCardVerificationNumber),
                        12,
                        2015,
                        Instrument::VISA,
                        GetTestShippingAddress().Pass());

  EXPECT_EQ(ASCIIToUTF16(kLastFourDigits), instrument.last_four_digits());
  EXPECT_TRUE(instrument.IsValid());
}

TEST(Instrument, NoPrimaryAccountNumberIsInvalid) {
  Instrument instrument(string16(),
                        ASCIIToUTF16(kCardVerificationNumber),
                        12,
                        2015,
                        Instrument::VISA,
                        GetTestShippingAddress().Pass());

  EXPECT_FALSE(instrument.IsValid());
}

TEST(Instrument, TooShortPrimaryAccountNumberIsInvalid) {
  Instrument instrument(ASCIIToUTF16("44447"),
                        ASCIIToUTF16(kCardVerificationNumber),
                        12,
                        2015,
                        Instrument::VISA,
                        GetTestShippingAddress().Pass());

  EXPECT_FALSE(instrument.IsValid());
}

TEST(Instrument, TooLongPrimaryAccountNumberIsInvalid) {
  Instrument instrument(ASCIIToUTF16("44444444444444444448"),
                        ASCIIToUTF16(kCardVerificationNumber),
                        12,
                        2015,
                        Instrument::VISA,
                        GetTestShippingAddress().Pass());

  EXPECT_FALSE(instrument.IsValid());
}

TEST(Instrument, PrimaryAccountNumberNotPassingLuhnIsInvalid) {
  Instrument instrument(ASCIIToUTF16("4444444444444444"),
                        ASCIIToUTF16(kCardVerificationNumber),
                        12,
                        2015,
                        Instrument::VISA,
                        GetTestShippingAddress().Pass());

  EXPECT_FALSE(instrument.IsValid());
}

TEST(Instrument, NoCardVerificationNumberIsInvalid) {
  Instrument instrument(ASCIIToUTF16(kPrimaryAccountNumber),
                        string16(),
                        12,
                        2015,
                        Instrument::VISA,
                        GetTestShippingAddress().Pass());

  EXPECT_FALSE(instrument.IsValid());
}

TEST(Instrument, TooShortCardVerificationNumberIsInvalid) {
  Instrument instrument(ASCIIToUTF16(kPrimaryAccountNumber),
                        ASCIIToUTF16("12"),
                        12,
                        2015,
                        Instrument::VISA,
                        GetTestShippingAddress().Pass());

  EXPECT_FALSE(instrument.IsValid());
}

TEST(Instrument, TooLongCardVerificationNumberIsInvalid) {
  Instrument instrument(ASCIIToUTF16(kPrimaryAccountNumber),
                        ASCIIToUTF16("12345"),
                        12,
                        2015,
                        Instrument::VISA,
                        GetTestShippingAddress().Pass());

  EXPECT_FALSE(instrument.IsValid());
}

TEST(Instrument, ZeroAsExpirationMonthIsInvalid) {
  Instrument instrument(ASCIIToUTF16(kPrimaryAccountNumber),
                        ASCIIToUTF16(kCardVerificationNumber),
                        0,
                        2015,
                        Instrument::VISA,
                        GetTestShippingAddress().Pass());

  EXPECT_FALSE(instrument.IsValid());
}

TEST(Instrument, TooLargeExpirationMonthIsInvalid) {
  Instrument instrument(ASCIIToUTF16(kPrimaryAccountNumber),
                        ASCIIToUTF16(kCardVerificationNumber),
                        13,
                        2015,
                        Instrument::VISA,
                        GetTestShippingAddress().Pass());

  EXPECT_FALSE(instrument.IsValid());
}

TEST(Instrument, TooSmallExpirationYearIsInvalid) {
  Instrument instrument(ASCIIToUTF16(kPrimaryAccountNumber),
                        ASCIIToUTF16(kCardVerificationNumber),
                        12,
                        999,
                        Instrument::VISA,
                        GetTestShippingAddress().Pass());

  EXPECT_FALSE(instrument.IsValid());
}

TEST(Instrument, TooLargeExpirationYearIsInvalid) {
  Instrument instrument(ASCIIToUTF16(kPrimaryAccountNumber),
                        ASCIIToUTF16(kCardVerificationNumber),
                        12,
                        10000,
                        Instrument::VISA,
                        GetTestShippingAddress().Pass());

  EXPECT_FALSE(instrument.IsValid());
}

TEST(Instrument, ToDictionary) {
  base::DictionaryValue expected;
  expected.SetString("type", "CREDIT_CARD");
  expected.SetInteger("credit_card.exp_month", 12);
  expected.SetInteger("credit_card.exp_year", 2015);
  expected.SetString("credit_card.last_4_digits", kLastFourDigits);
  expected.SetString("credit_card.fop_type", "VISA");
  expected.SetString("credit_card.address.country_name_code",
                     "ship_country_name_code");
  expected.SetString("credit_card.address.recipient_name",
                     "ship_recipient_name");
  expected.SetString("credit_card.address.locality_name",
                     "ship_locality_name");
  expected.SetString("credit_card.address.administrative_area_name",
                     "ship_admin_area_name");
  expected.SetString("credit_card.address.postal_code_number",
                     "ship_postal_code_number");
  base::ListValue* address_lines = new base::ListValue();
  address_lines->AppendString("ship_address_line_1");
  address_lines->AppendString("ship_address_line_2");
  expected.Set("credit_card.address.address_line", address_lines);

  Instrument instrument(ASCIIToUTF16(kPrimaryAccountNumber),
                        ASCIIToUTF16(kCardVerificationNumber),
                        12,
                        2015,
                        Instrument::VISA,
                        GetTestShippingAddress().Pass());

  EXPECT_TRUE(expected.Equals(instrument.ToDictionary().get()));
}

}  // namespace wallet
