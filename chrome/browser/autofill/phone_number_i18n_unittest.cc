// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/phone_number_i18n.h"
#include "testing/gtest/include/gtest/gtest.h"

using autofill_i18n::NormalizePhoneNumber;
using autofill_i18n::ParsePhoneNumber;
using autofill_i18n::ConstructPhoneNumber;
using autofill_i18n::FormatPhone;
using autofill_i18n::ComparePhones;
using autofill_i18n::PhoneNumbersMatch;

TEST(PhoneNumberI18NTest, NormalizePhoneNumber) {
  // The string is split to avoid problem with MSVC compiler when it thinks
  // 123 is a part of character code.
  string16 phone1(UTF8ToUTF16("\x92\x32" "123\xe2\x8a\x90"));
  EXPECT_EQ(NormalizePhoneNumber(phone1), ASCIIToUTF16("2123"));

  string16 phone2(UTF8ToUTF16(
      "\xef\xbc\x92\x32\x92\x37\xd9\xa9\xce\xb2\xe2\x8a\x90"));
  EXPECT_EQ(NormalizePhoneNumber(phone2), ASCIIToUTF16("2279"));

  string16 phone3(UTF8ToUTF16("\xef\xbc\x92\x35\xd9\xa5"));
  EXPECT_EQ(NormalizePhoneNumber(phone3), ASCIIToUTF16("255"));

  string16 phone4(UTF8ToUTF16("+1(650)2346789"));
  EXPECT_EQ(NormalizePhoneNumber(phone4), ASCIIToUTF16("16502346789"));

  string16 phone5(UTF8ToUTF16("6502346789"));
  EXPECT_EQ(NormalizePhoneNumber(phone5), ASCIIToUTF16("6502346789"));
}

TEST(PhoneNumberI18NTest, ParsePhoneNumber) {
  string16 number;
  string16 city_code;
  string16 country_code;

  // Test for empty string.  Should give back empty strings.
  string16 phone0;
  EXPECT_FALSE(ParsePhoneNumber(phone0, "US",
                                &country_code,
                                &city_code,
                                &number));
  EXPECT_EQ(string16(), number);
  EXPECT_EQ(string16(), city_code);
  EXPECT_EQ(string16(), country_code);

  // Test for string with less than 7 digits.  Should give back empty strings.
  string16 phone1(ASCIIToUTF16("1234"));
  EXPECT_FALSE(ParsePhoneNumber(phone1, "US",
                                &country_code,
                                &city_code,
                                &number));
  EXPECT_EQ(string16(), number);
  EXPECT_EQ(string16(), city_code);
  EXPECT_EQ(string16(), country_code);

  // Test for string with exactly 7 digits.
  // Not a valid number - starts with 1
  string16 phone2(ASCIIToUTF16("1234567"));
  EXPECT_FALSE(ParsePhoneNumber(phone2, "US",
                                &country_code,
                                &city_code,
                                &number));
  EXPECT_EQ(string16(), number);
  EXPECT_EQ(string16(), city_code);
  EXPECT_EQ(string16(), country_code);

  // Not a valid number - does not have area code.
  string16 phone3(ASCIIToUTF16("2234567"));
  EXPECT_FALSE(ParsePhoneNumber(phone3, "US",
                                &country_code,
                                &city_code,
                                &number));
  EXPECT_EQ(string16(), number);
  EXPECT_EQ(string16(), city_code);
  EXPECT_EQ(string16(), country_code);

  // Test for string with greater than 7 digits but less than 10 digits.
  // Should fail parsing in US.
  string16 phone4(ASCIIToUTF16("123456789"));
  EXPECT_FALSE(ParsePhoneNumber(phone4, "US",
                                &country_code,
                                &city_code,
                                &number));
  EXPECT_EQ(string16(), number);
  EXPECT_EQ(string16(), city_code);
  EXPECT_EQ(string16(), country_code);

  // Test for string with greater than 7 digits but less than 10 digits and
  // separators.
  // Should fail parsing in US.
  string16 phone_separator4(ASCIIToUTF16("12.345-6789"));
  EXPECT_FALSE(ParsePhoneNumber(phone_separator4, "US",
                                &country_code,
                                &city_code,
                                &number));
  EXPECT_EQ(string16(), number);
  EXPECT_EQ(string16(), city_code);
  EXPECT_EQ(string16(), country_code);

  // Test for string with exactly 10 digits.
  // Should give back phone number and city code.
  // This one going to fail because of the incorrect area code.
  string16 phone5(ASCIIToUTF16("1234567890"));
  EXPECT_FALSE(ParsePhoneNumber(phone5, "US",
                                &country_code,
                                &city_code,
                                &number));
  EXPECT_EQ(string16(), number);
  EXPECT_EQ(string16(), city_code);
  EXPECT_EQ(string16(), country_code);

  string16 phone6(ASCIIToUTF16("6501567890"));
  // This one going to fail because of the incorrect number (starts with 1).
  EXPECT_FALSE(ParsePhoneNumber(phone6, "US",
                                &country_code,
                                &city_code,
                                &number));
  EXPECT_EQ(string16(), number);
  EXPECT_EQ(string16(), city_code);
  EXPECT_EQ(string16(), country_code);

  string16 phone7(ASCIIToUTF16("6504567890"));
  EXPECT_TRUE(ParsePhoneNumber(phone7, "US",
                               &country_code,
                               &city_code,
                               &number));
  EXPECT_EQ(ASCIIToUTF16("4567890"), number);
  EXPECT_EQ(ASCIIToUTF16("650"), city_code);
  EXPECT_EQ(string16(), country_code);

  // Test for string with exactly 10 digits and separators.
  // Should give back phone number and city code.
  string16 phone_separator7(ASCIIToUTF16("(650) 456-7890"));
  EXPECT_TRUE(ParsePhoneNumber(phone_separator7, "US",
                               &country_code,
                               &city_code,
                               &number));
  EXPECT_EQ(ASCIIToUTF16("4567890"), number);
  EXPECT_EQ(ASCIIToUTF16("650"), city_code);
  EXPECT_EQ(string16(), country_code);

  // Tests for string with over 10 digits.
  // 01 is incorrect prefix in the USA, and if we interpret 011 as prefix, the
  // rest is too short for international number - the parsing should fail.
  string16 phone8(ASCIIToUTF16("0116504567890"));
  EXPECT_FALSE(ParsePhoneNumber(phone8, "US",
                                &country_code,
                                &city_code,
                                &number));
  EXPECT_EQ(string16(), number);
  EXPECT_EQ(string16(), city_code);
  EXPECT_EQ(string16(), country_code);

  // 011 is a correct "dial out" prefix in the USA - the parsing should succeed.
  string16 phone9(ASCIIToUTF16("01116504567890"));
  EXPECT_TRUE(ParsePhoneNumber(phone9, "US",
                               &country_code,
                               &city_code,
                               &number));
  EXPECT_EQ(ASCIIToUTF16("4567890"), number);
  EXPECT_EQ(ASCIIToUTF16("650"), city_code);
  EXPECT_EQ(ASCIIToUTF16("1"), country_code);

  // 011 is a correct "dial out" prefix in the USA - the parsing should succeed.
  string16 phone10(ASCIIToUTF16("01178124567890"));
  EXPECT_TRUE(ParsePhoneNumber(phone10, "US",
                               &country_code,
                               &city_code,
                               &number));
  EXPECT_EQ(ASCIIToUTF16("4567890"), number);
  EXPECT_EQ(ASCIIToUTF16("812"), city_code);
  EXPECT_EQ(ASCIIToUTF16("7"), country_code);

  // Test for string with over 10 digits with separator characters.
  // Should give back phone number, city code, and country code. "011" is
  // US "dial out" code, which is discarded.
  string16 phone11(ASCIIToUTF16("(0111) 650-456.7890"));
  EXPECT_TRUE(ParsePhoneNumber(phone11, "US",
                               &country_code,
                               &city_code,
                               &number));
  EXPECT_EQ(ASCIIToUTF16("4567890"), number);
  EXPECT_EQ(ASCIIToUTF16("650"), city_code);
  EXPECT_EQ(ASCIIToUTF16("1"), country_code);

  // Now try phone from Chech republic - it has 00 dial out code, 420 country
  // code and variable length area codes.
  string16 phone12(ASCIIToUTF16("+420 27-89.10.112"));
  EXPECT_TRUE(ParsePhoneNumber(phone12, "US",
                               &country_code,
                               &city_code,
                               &number));
  EXPECT_EQ(ASCIIToUTF16("278910112"), number);
  EXPECT_EQ(ASCIIToUTF16(""), city_code);
  EXPECT_EQ(ASCIIToUTF16("420"), country_code);

  EXPECT_TRUE(ParsePhoneNumber(phone12, "CZ",
                               &country_code,
                               &city_code,
                               &number));
  EXPECT_EQ(ASCIIToUTF16("278910112"), number);
  EXPECT_EQ(ASCIIToUTF16(""), city_code);
  EXPECT_EQ(ASCIIToUTF16("420"), country_code);

  string16 phone13(ASCIIToUTF16("420 57-89.10.112"));
  EXPECT_FALSE(ParsePhoneNumber(phone13, "US",
                                &country_code,
                                &city_code,
                                &number));
  EXPECT_TRUE(ParsePhoneNumber(phone13, "CZ",
                               &country_code,
                               &city_code,
                               &number));
  EXPECT_EQ(ASCIIToUTF16("578910112"), number);
  EXPECT_EQ(ASCIIToUTF16(""), city_code);
  EXPECT_EQ(ASCIIToUTF16("420"), country_code);

  string16 phone14(ASCIIToUTF16("1-650-FLOWERS"));
  EXPECT_TRUE(ParsePhoneNumber(phone14, "US",
                               &country_code,
                               &city_code,
                               &number));
  EXPECT_EQ(ASCIIToUTF16("3569377"), number);
  EXPECT_EQ(ASCIIToUTF16("650"), city_code);
  EXPECT_EQ(ASCIIToUTF16("1"), country_code);
}

TEST(PhoneNumberI18NTest, ConstructPhoneNumber) {
  string16 number;
  EXPECT_TRUE(ConstructPhoneNumber(ASCIIToUTF16("1"),
                                   ASCIIToUTF16("650"),
                                   ASCIIToUTF16("2345678"),
                                   "US",
                                   autofill_i18n::E164,
                                   &number));
  EXPECT_EQ(number, ASCIIToUTF16("+16502345678"));
  EXPECT_TRUE(ConstructPhoneNumber(ASCIIToUTF16("1"),
                                   ASCIIToUTF16("650"),
                                   ASCIIToUTF16("2345678"),
                                   "US",
                                   autofill_i18n::INTERNATIONAL,
                                   &number));
  EXPECT_EQ(number, ASCIIToUTF16("+1 650-234-5678"));
  EXPECT_TRUE(ConstructPhoneNumber(ASCIIToUTF16("1"),
                                   ASCIIToUTF16("650"),
                                   ASCIIToUTF16("2345678"),
                                   "US",
                                   autofill_i18n::NATIONAL,
                                   &number));
  EXPECT_EQ(number, ASCIIToUTF16("(650) 234-5678"));
  EXPECT_TRUE(ConstructPhoneNumber(ASCIIToUTF16("1"),
                                   ASCIIToUTF16("650"),
                                   ASCIIToUTF16("2345678"),
                                   "US",
                                   autofill_i18n::RFC3966,
                                   &number));
  EXPECT_EQ(number, ASCIIToUTF16("+1-650-234-5678"));
  EXPECT_TRUE(ConstructPhoneNumber(ASCIIToUTF16(""),
                                   ASCIIToUTF16("650"),
                                   ASCIIToUTF16("2345678"),
                                   "US",
                                   autofill_i18n::INTERNATIONAL,
                                   &number));
  EXPECT_EQ(number, ASCIIToUTF16("+1 650-234-5678"));
  EXPECT_TRUE(ConstructPhoneNumber(ASCIIToUTF16(""),
                                   ASCIIToUTF16(""),
                                   ASCIIToUTF16("6502345678"),
                                   "US",
                                   autofill_i18n::INTERNATIONAL,
                                   &number));
  EXPECT_EQ(number, ASCIIToUTF16("+1 650-234-5678"));

  EXPECT_FALSE(ConstructPhoneNumber(ASCIIToUTF16(""),
                                    ASCIIToUTF16("650"),
                                    ASCIIToUTF16("234567890"),
                                    "US",
                                    autofill_i18n::INTERNATIONAL,
                                    &number));
  EXPECT_EQ(number, string16());
  // Italian number
  EXPECT_TRUE(ConstructPhoneNumber(ASCIIToUTF16("39"),
                                   ASCIIToUTF16(""),
                                   ASCIIToUTF16("236618300"),
                                   "US",
                                   autofill_i18n::INTERNATIONAL,
                                   &number));
  EXPECT_EQ(number, ASCIIToUTF16("+39 236618300"));
  EXPECT_TRUE(ConstructPhoneNumber(ASCIIToUTF16("39"),
                                   ASCIIToUTF16(""),
                                   ASCIIToUTF16("236618300"),
                                   "US",
                                   autofill_i18n::NATIONAL,
                                   &number));
  EXPECT_EQ(number, ASCIIToUTF16("236618300"));
}

TEST(PhoneNumberI18NTest, FormatPhone) {
  EXPECT_EQ(FormatPhone(ASCIIToUTF16("1[650]234-56-78"), "US",
            autofill_i18n::NATIONAL),
            ASCIIToUTF16("(650) 234-5678"));
  EXPECT_EQ(FormatPhone(ASCIIToUTF16("(650)234-56-78"), "US",
            autofill_i18n::NATIONAL),
            ASCIIToUTF16("(650) 234-5678"));
  EXPECT_EQ(FormatPhone(ASCIIToUTF16("(650)234-56-78"), "US",
            autofill_i18n::INTERNATIONAL),
            ASCIIToUTF16("+1 650-234-5678"));
  EXPECT_EQ(FormatPhone(ASCIIToUTF16("01139236618300"), "US",
            autofill_i18n::INTERNATIONAL),
            ASCIIToUTF16("+39 236618300"));
  EXPECT_EQ(FormatPhone(ASCIIToUTF16("1(650)234-56-78"), "CZ",
            autofill_i18n::NATIONAL),
            ASCIIToUTF16("16502345678"));
  EXPECT_EQ(FormatPhone(ASCIIToUTF16("1(650)234-56-78"), "CZ",
            autofill_i18n::INTERNATIONAL),
            ASCIIToUTF16("+420 16502345678"));
}

TEST(PhoneNumberI18NTest, ComparePhones) {
  EXPECT_EQ(ComparePhones(ASCIIToUTF16("1(650)234-56-78"),
                          ASCIIToUTF16("+16502345678"),
                          "US"),
            autofill_i18n::PHONES_EQUAL);
  EXPECT_EQ(ComparePhones(ASCIIToUTF16("1(650)234-56-78"),
                          ASCIIToUTF16("6502345678"),
                          "US"),
            autofill_i18n::PHONES_EQUAL);
  EXPECT_EQ(ComparePhones(ASCIIToUTF16("1-800-FLOWERS"),
                          ASCIIToUTF16("18003569377"),
                          "US"),
            autofill_i18n::PHONES_EQUAL);
  EXPECT_EQ(ComparePhones(ASCIIToUTF16("1(650)234-56-78"),
                          ASCIIToUTF16("2345678"),
                          "US"),
            autofill_i18n::PHONES_SUBMATCH);
  EXPECT_EQ(ComparePhones(ASCIIToUTF16("234-56-78"),
                          ASCIIToUTF16("+16502345678"),
                          "US"),
            autofill_i18n::PHONES_SUBMATCH);
  EXPECT_EQ(ComparePhones(ASCIIToUTF16("1650234"),
                          ASCIIToUTF16("+16502345678"),
                          "US"),
            autofill_i18n::PHONES_NOT_EQUAL);
}

TEST(PhoneNumberI18NTest, PhoneNumbersMatch) {
  // Same numbers, defined country code.
  EXPECT_TRUE(PhoneNumbersMatch(ASCIIToUTF16("4158889999"),
                                ASCIIToUTF16("4158889999"),
                                "US"));
  // Same numbers, undefined country code.
  EXPECT_TRUE(PhoneNumbersMatch(ASCIIToUTF16("4158889999"),
                                ASCIIToUTF16("4158889999"),
                                ""));

  // Numbers differ by country code only.
  EXPECT_TRUE(PhoneNumbersMatch(ASCIIToUTF16("14158889999"),
                                ASCIIToUTF16("4158889999"),
                                "US"));

  // Same numbers, different formats.
  EXPECT_TRUE(PhoneNumbersMatch(ASCIIToUTF16("4158889999"),
                                ASCIIToUTF16("415-888-9999"),
                                "US"));
  EXPECT_TRUE(PhoneNumbersMatch(ASCIIToUTF16("4158889999"),
                                ASCIIToUTF16("(415)888-9999"),
                                "US"));
  EXPECT_TRUE(PhoneNumbersMatch(ASCIIToUTF16("4158889999"),
                                ASCIIToUTF16("415 888 9999"),
                                "US"));
  EXPECT_TRUE(PhoneNumbersMatch(ASCIIToUTF16("4158889999"),
                                ASCIIToUTF16("415 TUV WXYZ"),
                                "US"));

  // Partial matches don't count.
  EXPECT_FALSE(PhoneNumbersMatch(ASCIIToUTF16("14158889999"),
                                 ASCIIToUTF16("8889999"),
                                 "US"));
}
