// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/credit_card.h"
#include "chrome/browser/autofill/select_control_handler.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "webkit/glue/form_field.h"

TEST(SelectControlHandlerTest, CreditCardMonthExact) {
  const char* const kMonthsNumeric[] = {
    "01", "02", "03", "04", "05", "06", "07", "08", "09", "10", "11", "12",
  };
  std::vector<string16> options(arraysize(kMonthsNumeric));
  for (size_t i = 0; i < arraysize(kMonthsNumeric); ++i)
    options[i] = ASCIIToUTF16(kMonthsNumeric[i]);

  webkit_glue::FormField field;
  field.form_control_type = ASCIIToUTF16("select-one");
  field.option_strings = options;

  CreditCard credit_card;
  credit_card.SetInfo(AutofillType(CREDIT_CARD_EXP_MONTH), ASCIIToUTF16("01"));
  autofill::FillSelectControl(credit_card,
                              AutofillType(CREDIT_CARD_EXP_MONTH),
                              &field);
  EXPECT_EQ(ASCIIToUTF16("01"), field.value);
}

TEST(SelectControlHandlerTest, CreditCardMonthAbbreviated) {
  const char* const kMonthsAbbreviated[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec",
  };
  std::vector<string16> options(arraysize(kMonthsAbbreviated));
  for (size_t i = 0; i < arraysize(kMonthsAbbreviated); ++i)
    options[i] = ASCIIToUTF16(kMonthsAbbreviated[i]);

  webkit_glue::FormField field;
  field.form_control_type = ASCIIToUTF16("select-one");
  field.option_strings = options;

  CreditCard credit_card;
  credit_card.SetInfo(AutofillType(CREDIT_CARD_EXP_MONTH), ASCIIToUTF16("01"));
  autofill::FillSelectControl(credit_card,
                              AutofillType(CREDIT_CARD_EXP_MONTH),
                              &field);
  EXPECT_EQ(ASCIIToUTF16("Jan"), field.value);
}

TEST(SelectControlHandlerTest, CreditCardMonthFull) {
  const char* const kMonthsFull[] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December",
  };
  std::vector<string16> options(arraysize(kMonthsFull));
  for (size_t i = 0; i < arraysize(kMonthsFull); ++i)
    options[i] = ASCIIToUTF16(kMonthsFull[i]);

  webkit_glue::FormField field;
  field.form_control_type = ASCIIToUTF16("select-one");
  field.option_strings = options;

  CreditCard credit_card;
  credit_card.SetInfo(AutofillType(CREDIT_CARD_EXP_MONTH), ASCIIToUTF16("01"));
  autofill::FillSelectControl(credit_card,
                              AutofillType(CREDIT_CARD_EXP_MONTH),
                              &field);
  EXPECT_EQ(ASCIIToUTF16("January"), field.value);
}

TEST(SelectControlHandlerTest, CreditCardMonthNumeric) {
  const char* const kMonthsNumeric[] = {
    "1", "2", "3", "4", "5", "6", "7", "8", "9", "10", "11", "12",
  };
  std::vector<string16> options(arraysize(kMonthsNumeric));
  for (size_t i = 0; i < arraysize(kMonthsNumeric); ++i)
    options[i] = ASCIIToUTF16(kMonthsNumeric[i]);

  webkit_glue::FormField field;
  field.form_control_type = ASCIIToUTF16("select-one");
  field.option_strings = options;

  CreditCard credit_card;
  credit_card.SetInfo(AutofillType(CREDIT_CARD_EXP_MONTH), ASCIIToUTF16("01"));
  autofill::FillSelectControl(credit_card,
                              AutofillType(CREDIT_CARD_EXP_MONTH),
                              &field);
  EXPECT_EQ(ASCIIToUTF16("1"), field.value);
}

TEST(SelectControlHandlerTest, AddressCountryFull) {
  const char* const kCountries[] = {
    "Albania", "Canada"
  };
  std::vector<string16> options(arraysize(kCountries));
  for (size_t i = 0; i < arraysize(kCountries); ++i)
    options[i] = ASCIIToUTF16(kCountries[i]);

  webkit_glue::FormField field;
  field.form_control_type = ASCIIToUTF16("select-one");
  field.option_strings = options;

  AutoFillProfile profile;
  profile.SetInfo(AutofillType(ADDRESS_HOME_COUNTRY), ASCIIToUTF16("CA"));
  autofill::FillSelectControl(profile,
                              AutofillType(ADDRESS_HOME_COUNTRY),
                              &field);
  EXPECT_EQ(ASCIIToUTF16("Canada"), field.value);
}

TEST(SelectControlHandlerTest, AddressCountryAbbrev) {
  const char* const kCountries[] = {
    "AL", "CA"
  };
  std::vector<string16> options(arraysize(kCountries));
  for (size_t i = 0; i < arraysize(kCountries); ++i)
    options[i] = ASCIIToUTF16(kCountries[i]);

  webkit_glue::FormField field;
  field.form_control_type = ASCIIToUTF16("select-one");
  field.option_strings = options;

  AutoFillProfile profile;
  profile.SetInfo(AutofillType(ADDRESS_HOME_COUNTRY), ASCIIToUTF16("Canada"));
  autofill::FillSelectControl(profile,
                              AutofillType(ADDRESS_HOME_COUNTRY),
                              &field);
  EXPECT_EQ(ASCIIToUTF16("CA"), field.value);
}

TEST(SelectControlHandlerTest, AddressStateFull) {
  const char* const kStates[] = {
    "Alabama", "California"
  };
  std::vector<string16> options(arraysize(kStates));
  for (size_t i = 0; i < arraysize(kStates); ++i)
    options[i] = ASCIIToUTF16(kStates[i]);

  webkit_glue::FormField field;
  field.form_control_type = ASCIIToUTF16("select-one");
  field.option_strings = options;

  AutoFillProfile profile;
  profile.SetInfo(AutofillType(ADDRESS_HOME_STATE), ASCIIToUTF16("CA"));
  autofill::FillSelectControl(profile,
                              AutofillType(ADDRESS_HOME_STATE),
                              &field);
  EXPECT_EQ(ASCIIToUTF16("California"), field.value);
}

TEST(SelectControlHandlerTest, AddressStateAbbrev) {
  const char* const kStates[] = {
    "AL", "CA"
  };
  std::vector<string16> options(arraysize(kStates));
  for (size_t i = 0; i < arraysize(kStates); ++i)
    options[i] = ASCIIToUTF16(kStates[i]);

  webkit_glue::FormField field;
  field.form_control_type = ASCIIToUTF16("select-one");
  field.option_strings = options;

  AutoFillProfile profile;
  profile.SetInfo(AutofillType(ADDRESS_HOME_STATE), ASCIIToUTF16("California"));
  autofill::FillSelectControl(profile,
                              AutofillType(ADDRESS_HOME_STATE),
                              &field);
  EXPECT_EQ(ASCIIToUTF16("CA"), field.value);
}
