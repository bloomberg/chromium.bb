// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/field_types.h"
#include "chrome/browser/autofill/phone_number.h"
#include "chrome/browser/autofill/phone_number_i18n.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(PhoneNumberTest, Matcher) {
  AutofillProfile profile;
  profile.SetCountryCode("US");
  // Set phone number so country_code == 1, city_code = 650, number = 2345678.
  string16 phone(ASCIIToUTF16("1 [650] 234-5678"));
  PhoneNumber phone_number(&profile);
  phone_number.SetCanonicalizedInfo(PHONE_HOME_WHOLE_NUMBER, phone);

  FieldTypeSet matching_types;
  phone_number.GetMatchingTypes(string16(), &matching_types);
  EXPECT_EQ(1U, matching_types.size());
  EXPECT_TRUE(matching_types.find(EMPTY_TYPE) != matching_types.end());
  matching_types.clear();
  phone_number.GetMatchingTypes(ASCIIToUTF16("1"), &matching_types);
  EXPECT_EQ(1U, matching_types.size());
  EXPECT_TRUE(matching_types.find(PHONE_HOME_COUNTRY_CODE) !=
              matching_types.end());
  matching_types.clear();
  phone_number.GetMatchingTypes(ASCIIToUTF16("16"), &matching_types);
  EXPECT_EQ(0U, matching_types.size());
  phone_number.GetMatchingTypes(ASCIIToUTF16("165"), &matching_types);
  EXPECT_EQ(0U, matching_types.size());
  phone_number.GetMatchingTypes(ASCIIToUTF16("1650"), &matching_types);
  EXPECT_EQ(0U, matching_types.size());
  phone_number.GetMatchingTypes(ASCIIToUTF16("16502"), &matching_types);
  EXPECT_EQ(0U, matching_types.size());
  phone_number.GetMatchingTypes(ASCIIToUTF16("165023"), &matching_types);
  EXPECT_EQ(0U, matching_types.size());
  phone_number.GetMatchingTypes(ASCIIToUTF16("1650234"), &matching_types);
  EXPECT_EQ(0U, matching_types.size());
  matching_types.clear();
  phone_number.GetMatchingTypes(ASCIIToUTF16("16502345678"), &matching_types);
  EXPECT_EQ(1U, matching_types.size());
  EXPECT_TRUE(matching_types.find(PHONE_HOME_WHOLE_NUMBER) !=
              matching_types.end());
  matching_types.clear();
  phone_number.GetMatchingTypes(ASCIIToUTF16("650"), &matching_types);
  EXPECT_EQ(1U, matching_types.size());
  EXPECT_TRUE(matching_types.find(PHONE_HOME_CITY_CODE) !=
              matching_types.end());
  matching_types.clear();
  phone_number.GetMatchingTypes(ASCIIToUTF16("2345678"), &matching_types);
  EXPECT_EQ(1U, matching_types.size());
  EXPECT_TRUE(matching_types.find(PHONE_HOME_NUMBER) != matching_types.end());
  matching_types.clear();
  phone_number.GetMatchingTypes(ASCIIToUTF16("234"), &matching_types);
  EXPECT_EQ(1U, matching_types.size());
  EXPECT_TRUE(matching_types.find(PHONE_HOME_NUMBER) != matching_types.end());
  matching_types.clear();
  phone_number.GetMatchingTypes(ASCIIToUTF16("5678"), &matching_types);
  EXPECT_EQ(1U, matching_types.size());
  EXPECT_TRUE(matching_types.find(PHONE_HOME_NUMBER) != matching_types.end());
  matching_types.clear();
  phone_number.GetMatchingTypes(ASCIIToUTF16("2345"), &matching_types);
  EXPECT_EQ(0U, matching_types.size());
  matching_types.clear();
  phone_number.GetMatchingTypes(ASCIIToUTF16("6502345678"), &matching_types);
  EXPECT_EQ(1U, matching_types.size());
  EXPECT_TRUE(matching_types.find(PHONE_HOME_CITY_AND_NUMBER) !=
              matching_types.end());
  matching_types.clear();
  phone_number.GetMatchingTypes(ASCIIToUTF16("(650)2345678"), &matching_types);
  EXPECT_EQ(1U, matching_types.size());
  EXPECT_TRUE(matching_types.find(PHONE_HOME_CITY_AND_NUMBER) !=
              matching_types.end());
}

TEST(PhoneNumberTest, PhoneCombineHelper) {
  PhoneNumber::PhoneCombineHelper number1;
  EXPECT_FALSE(number1.SetInfo(ADDRESS_BILLING_CITY,
                               ASCIIToUTF16("1")));
  EXPECT_TRUE(number1.SetInfo(PHONE_HOME_COUNTRY_CODE,
                              ASCIIToUTF16("1")));
  EXPECT_TRUE(number1.SetInfo(PHONE_HOME_CITY_CODE,
                              ASCIIToUTF16("650")));
  EXPECT_TRUE(number1.SetInfo(PHONE_HOME_NUMBER,
                              ASCIIToUTF16("2345678")));
  string16 parsed_phone;
  EXPECT_TRUE(number1.ParseNumber("US", &parsed_phone));
  // International format as it has a country code.
  EXPECT_EQ(ASCIIToUTF16("+1 650-234-5678"), parsed_phone);

  PhoneNumber::PhoneCombineHelper number3;
  EXPECT_TRUE(number3.SetInfo(PHONE_HOME_CITY_CODE,
                              ASCIIToUTF16("650")));
  EXPECT_TRUE(number3.SetInfo(PHONE_HOME_NUMBER,
                              ASCIIToUTF16("2345680")));
  EXPECT_TRUE(number3.ParseNumber("US", &parsed_phone));
  // National format as it does not have a country code.
  EXPECT_EQ(ASCIIToUTF16("(650) 234-5680"), parsed_phone);

  PhoneNumber::PhoneCombineHelper number4;
  EXPECT_TRUE(number4.SetInfo(PHONE_HOME_CITY_CODE,
                              ASCIIToUTF16("123")));  // Incorrect city code.
  EXPECT_TRUE(number4.SetInfo(PHONE_HOME_NUMBER,
                              ASCIIToUTF16("2345680")));
  EXPECT_FALSE(number4.ParseNumber("US", &parsed_phone));
  EXPECT_EQ(string16(), parsed_phone);

  PhoneNumber::PhoneCombineHelper number5;
  EXPECT_TRUE(number5.SetInfo(PHONE_HOME_CITY_AND_NUMBER,
                              ASCIIToUTF16("6502345681")));
  EXPECT_TRUE(number5.ParseNumber("US", &parsed_phone));
  EXPECT_EQ(ASCIIToUTF16("(650) 234-5681"), parsed_phone);

  PhoneNumber::PhoneCombineHelper number6;
  EXPECT_TRUE(number6.SetInfo(PHONE_HOME_CITY_CODE,
                              ASCIIToUTF16("650")));
  EXPECT_TRUE(number6.SetInfo(PHONE_HOME_NUMBER,
                              ASCIIToUTF16("234")));
  EXPECT_TRUE(number6.SetInfo(PHONE_HOME_NUMBER,
                              ASCIIToUTF16("5682")));
  EXPECT_TRUE(number6.ParseNumber("US", &parsed_phone));
  EXPECT_EQ(ASCIIToUTF16("(650) 234-5682"), parsed_phone);
}
