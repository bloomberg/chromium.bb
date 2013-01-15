// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/phone_number_i18n.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libphonenumber/src/phonenumber_api.h"

using autofill_i18n::NormalizePhoneNumber;
using autofill_i18n::ParsePhoneNumber;
using autofill_i18n::ConstructPhoneNumber;
using autofill_i18n::PhoneNumbersMatch;
using content::BrowserThread;

TEST(PhoneNumberI18NTest, NormalizePhoneNumber) {
  // "Large" digits.
  string16 phone1(UTF8ToUTF16("\xEF\xBC\x91\xEF\xBC\x96\xEF\xBC\x95\xEF\xBC\x90"
                              "\xEF\xBC\x97\xEF\xBC\x94\xEF\xBC\x99\xEF\xBC\x98"
                              "\xEF\xBC\x93\xEF\xBC\x92\xEF\xBC\x93"));
  EXPECT_EQ(NormalizePhoneNumber(phone1, "US"), ASCIIToUTF16("16507498323"));

  // Devanagari script digits.
  string16 phone2(UTF8ToUTF16("\xD9\xA1\xD9\xA6\xD9\xA5\xD9\xA0\xD9\xA8\xD9\xA3"
                              "\xD9\xA2\xD9\xA3\xD9\xA7\xD9\xA4\xD9\xA9"));
  EXPECT_EQ(NormalizePhoneNumber(phone2, "US"), ASCIIToUTF16("16508323749"));

  string16 phone3(UTF8ToUTF16("16503334\xef\xbc\x92\x35\xd9\xa5"));
  EXPECT_EQ(NormalizePhoneNumber(phone3, "US"), ASCIIToUTF16("16503334255"));

  string16 phone4(UTF8ToUTF16("+1(650)2346789"));
  EXPECT_EQ(NormalizePhoneNumber(phone4, "US"), ASCIIToUTF16("16502346789"));

  string16 phone5(UTF8ToUTF16("6502346789"));
  EXPECT_EQ(NormalizePhoneNumber(phone5, "US"), ASCIIToUTF16("6502346789"));
}

TEST(PhoneNumberI18NTest, ParsePhoneNumber) {
  string16 number;
  string16 city_code;
  string16 country_code;
  i18n::phonenumbers::PhoneNumber unused_i18n_number;

  // Test for empty string.  Should give back empty strings.
  string16 phone0;
  EXPECT_FALSE(ParsePhoneNumber(phone0, "US",
                                &country_code,
                                &city_code,
                                &number,
                                &unused_i18n_number));
  EXPECT_EQ(string16(), number);
  EXPECT_EQ(string16(), city_code);
  EXPECT_EQ(string16(), country_code);

  // Test for string with less than 7 digits.  Should give back empty strings.
  string16 phone1(ASCIIToUTF16("1234"));
  EXPECT_FALSE(ParsePhoneNumber(phone1, "US",
                                &country_code,
                                &city_code,
                                &number,
                                &unused_i18n_number));
  EXPECT_EQ(string16(), number);
  EXPECT_EQ(string16(), city_code);
  EXPECT_EQ(string16(), country_code);

  // Test for string with exactly 7 digits.
  // Not a valid number - starts with 1
  string16 phone2(ASCIIToUTF16("1234567"));
  EXPECT_FALSE(ParsePhoneNumber(phone2, "US",
                                &country_code,
                                &city_code,
                                &number,
                                &unused_i18n_number));
  EXPECT_EQ(string16(), number);
  EXPECT_EQ(string16(), city_code);
  EXPECT_EQ(string16(), country_code);

  // Not a valid number - does not have area code.
  string16 phone3(ASCIIToUTF16("2234567"));
  EXPECT_FALSE(ParsePhoneNumber(phone3, "US",
                                &country_code,
                                &city_code,
                                &number,
                                &unused_i18n_number));
  EXPECT_EQ(string16(), number);
  EXPECT_EQ(string16(), city_code);
  EXPECT_EQ(string16(), country_code);

  // Test for string with greater than 7 digits but less than 10 digits.
  // Should fail parsing in US.
  string16 phone4(ASCIIToUTF16("123456789"));
  EXPECT_FALSE(ParsePhoneNumber(phone4, "US",
                                &country_code,
                                &city_code,
                                &number,
                                &unused_i18n_number));
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
                                &number,
                                &unused_i18n_number));
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
                                &number,
                                &unused_i18n_number));
  EXPECT_EQ(string16(), number);
  EXPECT_EQ(string16(), city_code);
  EXPECT_EQ(string16(), country_code);

  string16 phone6(ASCIIToUTF16("6501567890"));
  // This one going to fail because of the incorrect number (starts with 1).
  EXPECT_FALSE(ParsePhoneNumber(phone6, "US",
                                &country_code,
                                &city_code,
                                &number,
                                &unused_i18n_number));
  EXPECT_EQ(string16(), number);
  EXPECT_EQ(string16(), city_code);
  EXPECT_EQ(string16(), country_code);

  string16 phone7(ASCIIToUTF16("6504567890"));
  EXPECT_TRUE(ParsePhoneNumber(phone7, "US",
                               &country_code,
                               &city_code,
                               &number,
                               &unused_i18n_number));
  EXPECT_EQ(ASCIIToUTF16("4567890"), number);
  EXPECT_EQ(ASCIIToUTF16("650"), city_code);
  EXPECT_EQ(string16(), country_code);

  // Test for string with exactly 10 digits and separators.
  // Should give back phone number and city code.
  string16 phone_separator7(ASCIIToUTF16("(650) 456-7890"));
  EXPECT_TRUE(ParsePhoneNumber(phone_separator7, "US",
                               &country_code,
                               &city_code,
                               &number,
                               &unused_i18n_number));
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
                                &number,
                                &unused_i18n_number));
  EXPECT_EQ(string16(), number);
  EXPECT_EQ(string16(), city_code);
  EXPECT_EQ(string16(), country_code);

  // 011 is a correct "dial out" prefix in the USA - the parsing should succeed.
  string16 phone9(ASCIIToUTF16("01116504567890"));
  EXPECT_TRUE(ParsePhoneNumber(phone9, "US",
                               &country_code,
                               &city_code,
                               &number,
                               &unused_i18n_number));
  EXPECT_EQ(ASCIIToUTF16("4567890"), number);
  EXPECT_EQ(ASCIIToUTF16("650"), city_code);
  EXPECT_EQ(ASCIIToUTF16("1"), country_code);

  // 011 is a correct "dial out" prefix in the USA - the parsing should succeed.
  string16 phone10(ASCIIToUTF16("01178124567890"));
  EXPECT_TRUE(ParsePhoneNumber(phone10, "US",
                               &country_code,
                               &city_code,
                               &number,
                               &unused_i18n_number));
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
                               &number,
                               &unused_i18n_number));
  EXPECT_EQ(ASCIIToUTF16("4567890"), number);
  EXPECT_EQ(ASCIIToUTF16("650"), city_code);
  EXPECT_EQ(ASCIIToUTF16("1"), country_code);

  // Now try phone from Chech republic - it has 00 dial out code, 420 country
  // code and variable length area codes.
  string16 phone12(ASCIIToUTF16("+420 27-89.10.112"));
  EXPECT_TRUE(ParsePhoneNumber(phone12, "US",
                               &country_code,
                               &city_code,
                               &number,
                               &unused_i18n_number));
  EXPECT_EQ(ASCIIToUTF16("910112"), number);
  EXPECT_EQ(ASCIIToUTF16("278"), city_code);
  EXPECT_EQ(ASCIIToUTF16("420"), country_code);

  EXPECT_TRUE(ParsePhoneNumber(phone12, "CZ",
                               &country_code,
                               &city_code,
                               &number,
                               &unused_i18n_number));
  EXPECT_EQ(ASCIIToUTF16("910112"), number);
  EXPECT_EQ(ASCIIToUTF16("278"), city_code);
  EXPECT_EQ(ASCIIToUTF16("420"), country_code);

  string16 phone13(ASCIIToUTF16("420 57-89.10.112"));
  EXPECT_FALSE(ParsePhoneNumber(phone13, "US",
                                &country_code,
                                &city_code,
                                &number,
                                &unused_i18n_number));
  EXPECT_TRUE(ParsePhoneNumber(phone13, "CZ",
                               &country_code,
                               &city_code,
                               &number,
                               &unused_i18n_number));
  EXPECT_EQ(ASCIIToUTF16("910112"), number);
  EXPECT_EQ(ASCIIToUTF16("578"), city_code);
  EXPECT_EQ(ASCIIToUTF16("420"), country_code);

  string16 phone14(ASCIIToUTF16("1-650-FLOWERS"));
  EXPECT_TRUE(ParsePhoneNumber(phone14, "US",
                               &country_code,
                               &city_code,
                               &number,
                               &unused_i18n_number));
  EXPECT_EQ(ASCIIToUTF16("3569377"), number);
  EXPECT_EQ(ASCIIToUTF16("650"), city_code);
  EXPECT_EQ(ASCIIToUTF16("1"), country_code);

  // 800 is not an area code, but the destination code. In our library these
  // codes should be treated the same as area codes.
  string16 phone15(ASCIIToUTF16("1-800-FLOWERS"));
  EXPECT_TRUE(ParsePhoneNumber(phone15, "US",
                               &country_code,
                               &city_code,
                               &number,
                               &unused_i18n_number));
  EXPECT_EQ(ASCIIToUTF16("3569377"), number);
  EXPECT_EQ(ASCIIToUTF16("800"), city_code);
  EXPECT_EQ(ASCIIToUTF16("1"), country_code);
}

TEST(PhoneNumberI18NTest, ConstructPhoneNumber) {
  string16 number;
  EXPECT_TRUE(ConstructPhoneNumber(ASCIIToUTF16("1"),
                                   ASCIIToUTF16("650"),
                                   ASCIIToUTF16("2345678"),
                                   "US",
                                   &number));
  EXPECT_EQ(number, ASCIIToUTF16("+1 650-234-5678"));
  EXPECT_TRUE(ConstructPhoneNumber(string16(),
                                   ASCIIToUTF16("650"),
                                   ASCIIToUTF16("2345678"),
                                   "US",
                                   &number));
  EXPECT_EQ(number, ASCIIToUTF16("(650) 234-5678"));
  EXPECT_TRUE(ConstructPhoneNumber(ASCIIToUTF16("1"),
                                   string16(),
                                   ASCIIToUTF16("6502345678"),
                                   "US",
                                   &number));
  EXPECT_EQ(number, ASCIIToUTF16("+1 650-234-5678"));
  EXPECT_TRUE(ConstructPhoneNumber(string16(),
                                   string16(),
                                   ASCIIToUTF16("6502345678"),
                                   "US",
                                   &number));
  EXPECT_EQ(number, ASCIIToUTF16("(650) 234-5678"));

  EXPECT_FALSE(ConstructPhoneNumber(string16(),
                                    ASCIIToUTF16("650"),
                                    ASCIIToUTF16("234567890"),
                                    "US",
                                    &number));
  EXPECT_EQ(number, string16());
  // Italian number
  EXPECT_TRUE(ConstructPhoneNumber(ASCIIToUTF16("39"),
                                   ASCIIToUTF16("347"),
                                   ASCIIToUTF16("2345678"),
                                   "IT",
                                   &number));
  EXPECT_EQ(number, ASCIIToUTF16("+39 347 234 5678"));
  EXPECT_TRUE(ConstructPhoneNumber(string16(),
                                   ASCIIToUTF16("347"),
                                   ASCIIToUTF16("2345678"),
                                   "IT",
                                   &number));
  EXPECT_EQ(number, ASCIIToUTF16("347 234 5678"));
  // German number.
  EXPECT_TRUE(ConstructPhoneNumber(ASCIIToUTF16("49"),
                                   ASCIIToUTF16("024"),
                                   ASCIIToUTF16("2345678901"),
                                   "DE",
                                   &number));
  EXPECT_EQ(number, ASCIIToUTF16("+49 2423/45678901"));
  EXPECT_TRUE(ConstructPhoneNumber(string16(),
                                   ASCIIToUTF16("024"),
                                   ASCIIToUTF16("2345678901"),
                                   "DE",
                                   &number));
  EXPECT_EQ(number, ASCIIToUTF16("02423/45678901"));
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
  EXPECT_TRUE(PhoneNumbersMatch(ASCIIToUTF16("1(415)888-99-99"),
                                ASCIIToUTF16("+14158889999"),
                                "US"));

  // Partial matches don't count.
  EXPECT_FALSE(PhoneNumbersMatch(ASCIIToUTF16("14158889999"),
                                 ASCIIToUTF16("8889999"),
                                 "US"));

  // Different numbers don't match.
  EXPECT_FALSE(PhoneNumbersMatch(ASCIIToUTF16("14158889999"),
                                 ASCIIToUTF16("1415888"),
                                 "US"));
}
