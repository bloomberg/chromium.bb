// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/home_phone_number.h"
#include "chrome/browser/autofill/phone_number.h"
#include "testing/gtest/include/gtest/gtest.h"

// Tests the phone number parser.
TEST(PhoneNumberTest, Parser) {
  string16 number;
  string16 city_code;
  string16 country_code;

  // Test for empty string.  Should give back empty strings.
  string16 phone0;
  PhoneNumber::ParsePhoneNumber(phone0, &number, &city_code, &country_code);
  EXPECT_EQ(string16(), number);
  EXPECT_EQ(string16(), city_code);
  EXPECT_EQ(string16(), country_code);

  // Test for string with less than 7 digits.  Should give back empty strings.
  string16 phone1(ASCIIToUTF16("1234"));
  PhoneNumber::ParsePhoneNumber(phone1, &number, &city_code, &country_code);
  EXPECT_EQ(string16(), number);
  EXPECT_EQ(string16(), city_code);
  EXPECT_EQ(string16(), country_code);

  // Test for string with exactly 7 digits.  Should give back only phone number.
  string16 phone2(ASCIIToUTF16("1234567"));
  PhoneNumber::ParsePhoneNumber(phone2, &number, &city_code, &country_code);
  EXPECT_EQ(ASCIIToUTF16("1234567"), number);
  EXPECT_EQ(string16(), city_code);
  EXPECT_EQ(string16(), country_code);

  // Test for string with exactly 7 digits and separators.  Should give back
  // only phone number.
  string16 phone_separator2(ASCIIToUTF16("123-4567"));
  PhoneNumber::ParsePhoneNumber(phone_separator2,
      &number, &city_code, &country_code);
  EXPECT_EQ(ASCIIToUTF16("1234567"), number);
  EXPECT_EQ(string16(), city_code);
  EXPECT_EQ(string16(), country_code);

  // Test for string with greater than 7 digits but less than 10 digits.
  // Should give back only phone number.
  string16 phone3(ASCIIToUTF16("123456789"));
  PhoneNumber::ParsePhoneNumber(phone3, &number, &city_code, &country_code);
  EXPECT_EQ(ASCIIToUTF16("3456789"), number);
  EXPECT_EQ(string16(), city_code);
  EXPECT_EQ(string16(), country_code);

  // Test for string with greater than 7 digits but less than 10 digits and
  // separators.
  // Should give back only phone number.
  string16 phone_separator3(ASCIIToUTF16("12.345-6789"));
  PhoneNumber::ParsePhoneNumber(phone3, &number, &city_code, &country_code);
  EXPECT_EQ(ASCIIToUTF16("3456789"), number);
  EXPECT_EQ(string16(), city_code);
  EXPECT_EQ(string16(), country_code);

  // Test for string with exactly 10 digits.
  // Should give back phone number and city code.
  string16 phone4(ASCIIToUTF16("1234567890"));
  PhoneNumber::ParsePhoneNumber(phone4, &number, &city_code, &country_code);
  EXPECT_EQ(ASCIIToUTF16("4567890"), number);
  EXPECT_EQ(ASCIIToUTF16("123"), city_code);
  EXPECT_EQ(string16(), country_code);

  // Test for string with exactly 10 digits and separators.
  // Should give back phone number and city code.
  string16 phone_separator4(ASCIIToUTF16("(123) 456-7890"));
  PhoneNumber::ParsePhoneNumber(phone_separator4,
      &number, &city_code, &country_code);
  EXPECT_EQ(ASCIIToUTF16("4567890"), number);
  EXPECT_EQ(ASCIIToUTF16("123"), city_code);
  EXPECT_EQ(string16(), country_code);

  // Test for string with over 10 digits.
  // Should give back phone number, city code, and country code.
  string16 phone5(ASCIIToUTF16("011234567890"));
  PhoneNumber::ParsePhoneNumber(phone5, &number, &city_code, &country_code);
  EXPECT_EQ(ASCIIToUTF16("4567890"), number);
  EXPECT_EQ(ASCIIToUTF16("123"), city_code);
  EXPECT_EQ(ASCIIToUTF16("01"), country_code);

  // Test for string with over 10 digits with separator characters.
  // Should give back phone number, city code, and country code.
  string16 phone6(ASCIIToUTF16("(01) 123-456.7890"));
  PhoneNumber::ParsePhoneNumber(phone6, &number, &city_code, &country_code);
  EXPECT_EQ(ASCIIToUTF16("4567890"), number);
  EXPECT_EQ(ASCIIToUTF16("123"), city_code);
  EXPECT_EQ(ASCIIToUTF16("01"), country_code);
}

TEST(PhoneNumberTest, Matcher) {
  // Set phone number so country_code == 12, city_code = 123, number = 1234567.
  string16 phone(ASCIIToUTF16("121231234567"));
  HomePhoneNumber phone_number;
  phone_number.set_whole_number(phone);

  EXPECT_FALSE(phone_number.IsCountryCode(ASCIIToUTF16("")));
  EXPECT_FALSE(phone_number.IsCountryCode(ASCIIToUTF16("1")));
  EXPECT_TRUE(phone_number.IsCountryCode(ASCIIToUTF16("12")));
  EXPECT_FALSE(phone_number.IsCountryCode(ASCIIToUTF16("123")));

  EXPECT_FALSE(phone_number.IsCityCode(ASCIIToUTF16("")));
  EXPECT_FALSE(phone_number.IsCityCode(ASCIIToUTF16("1")));
  EXPECT_FALSE(phone_number.IsCityCode(ASCIIToUTF16("12")));
  EXPECT_TRUE(phone_number.IsCityCode(ASCIIToUTF16("123")));
  EXPECT_FALSE(phone_number.IsCityCode(ASCIIToUTF16("1234")));

  EXPECT_FALSE(phone_number.IsNumber(ASCIIToUTF16("")));
  EXPECT_FALSE(phone_number.IsNumber(ASCIIToUTF16("1")));
  EXPECT_FALSE(phone_number.IsNumber(ASCIIToUTF16("12")));
  EXPECT_TRUE(phone_number.IsNumber(ASCIIToUTF16("123")));
  EXPECT_FALSE(phone_number.IsNumber(ASCIIToUTF16("1234")));
  EXPECT_FALSE(phone_number.IsNumber(ASCIIToUTF16("12345")));
  EXPECT_FALSE(phone_number.IsNumber(ASCIIToUTF16("123456")));
  EXPECT_TRUE(phone_number.IsNumber(ASCIIToUTF16("1234567")));
  EXPECT_FALSE(phone_number.IsNumber(ASCIIToUTF16("234567")));
  EXPECT_FALSE(phone_number.IsNumber(ASCIIToUTF16("34567")));
  EXPECT_TRUE(phone_number.IsNumber(ASCIIToUTF16("4567")));
  EXPECT_FALSE(phone_number.IsNumber(ASCIIToUTF16("567")));
  EXPECT_FALSE(phone_number.IsNumber(ASCIIToUTF16("67")));
  EXPECT_FALSE(phone_number.IsNumber(ASCIIToUTF16("7")));
}
