// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>

#include "base/macros.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/phone_number_i18n.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libphonenumber/phonenumber_api.h"

using base::ASCIIToUTF16;
using base::UTF8ToUTF16;

namespace autofill {

using i18n::NormalizePhoneNumber;
using i18n::ParsePhoneNumber;
using i18n::ConstructPhoneNumber;
using i18n::PhoneNumbersMatch;

TEST(PhoneNumberI18NTest, NormalizePhoneNumber) {
  // "Large" digits.
  base::string16 phone1(UTF8ToUTF16(
      "\xEF\xBC\x91\xEF\xBC\x96\xEF\xBC\x95\xEF\xBC\x90"
      "\xEF\xBC\x97\xEF\xBC\x94\xEF\xBC\x99\xEF\xBC\x98"
      "\xEF\xBC\x93\xEF\xBC\x92\xEF\xBC\x93"));
  EXPECT_EQ(NormalizePhoneNumber(phone1, "US"), ASCIIToUTF16("16507498323"));

  // Devanagari script digits.
  base::string16 phone2(UTF8ToUTF16(
      "\xD9\xA1\xD9\xA6\xD9\xA5\xD9\xA0\xD9\xA8\xD9\xA3"
      "\xD9\xA2\xD9\xA3\xD9\xA7\xD9\xA4\xD9\xA9"));
  EXPECT_EQ(NormalizePhoneNumber(phone2, "US"), ASCIIToUTF16("16508323749"));

  base::string16 phone3(UTF8ToUTF16("16503334\xef\xbc\x92\x35\xd9\xa5"));
  EXPECT_EQ(NormalizePhoneNumber(phone3, "US"), ASCIIToUTF16("16503334255"));

  base::string16 phone4(UTF8ToUTF16("+1(650)2346789"));
  EXPECT_EQ(NormalizePhoneNumber(phone4, "US"), ASCIIToUTF16("16502346789"));

  base::string16 phone5(UTF8ToUTF16("6502346789"));
  EXPECT_EQ(NormalizePhoneNumber(phone5, "US"), ASCIIToUTF16("6502346789"));
}

struct ParseNumberTestCase {
  // Expected parsing result.
  bool valid;
  // Inputs.
  std::string input;
  std::string assumed_region;
  // Further expectations.
  std::string number;
  std::string city_code;
  std::string country_code;
  std::string deduced_region;
};

class ParseNumberTest : public testing::TestWithParam<ParseNumberTestCase> {};

TEST_P(ParseNumberTest, ParsePhoneNumber) {
  auto test_case = GetParam();
  SCOPED_TRACE("Testing phone number " + test_case.input);

  base::string16 country_code, city_code, number;
  std::string deduced_region;
  ::i18n::phonenumbers::PhoneNumber unused_i18n_number;
  EXPECT_EQ(
      test_case.valid,
      ParsePhoneNumber(ASCIIToUTF16(test_case.input), test_case.assumed_region,
                       &country_code, &city_code, &number, &deduced_region,
                       &unused_i18n_number));
  EXPECT_EQ(ASCIIToUTF16(test_case.number), number);
  EXPECT_EQ(ASCIIToUTF16(test_case.city_code), city_code);
  EXPECT_EQ(ASCIIToUTF16(test_case.country_code), country_code);
  EXPECT_EQ(test_case.deduced_region, deduced_region);
}

INSTANTIATE_TEST_CASE_P(
    PhoneNumberI18NTest,
    ParseNumberTest,
    testing::Values(
        // Test for empty string.  Should give back empty strings.
        ParseNumberTestCase{false, "", "US"},
        // Test for string with less than 7 digits.  Should give back empty
        // strings.
        ParseNumberTestCase{false, "1234", "US"},
        // Test for string with exactly 7 digits.
        // Not a valid number - starts with 1
        ParseNumberTestCase{false, "17134567", "US"},
        // Not a valid number - does not have area code.
        ParseNumberTestCase{false, "7134567", "US"},
        // Valid Canadian toll-free number.
        ParseNumberTestCase{true, "3101234", "US", "3101234", "", "", "CA"},
        // Test for string with greater than 7 digits but less than 10 digits.
        // Should fail parsing in US.
        ParseNumberTestCase{false, "123456789", "US"},
        // Test for string with greater than 7 digits but less than 10 digits
        // and
        // separators.
        // Should fail parsing in US.
        ParseNumberTestCase{false, "12.345-6789", "US"},
        // Test for string with exactly 10 digits.
        // Should give back phone number and city code.
        // This one going to fail because of the incorrect area code.
        ParseNumberTestCase{false, "1234567890", "US"},
        // This one going to fail because of the incorrect number (starts with
        // 1).
        ParseNumberTestCase{false, "6501567890", "US"},
        ParseNumberTestCase{true, "6504567890", "US", "4567890", "650", "",
                            "US"},
        // Test for string with exactly 10 digits and separators.
        // Should give back phone number and city code.
        ParseNumberTestCase{true, "(650) 456-7890", "US", "4567890", "650", "",
                            "US"},
        // Tests for string with over 10 digits.
        // 01 is incorrect prefix in the USA, and if we interpret 011 as prefix,
        // the
        // rest is too short for international number - the parsing should fail.
        ParseNumberTestCase{false, "0116504567890", "US"},
        // 011 is a correct "dial out" prefix in the USA - the parsing should
        // succeed.
        ParseNumberTestCase{true, "01116504567890", "US", "4567890", "650", "1",
                            "US"},
        // 011 is a correct "dial out" prefix in the USA but the rest of the
        // number
        // can't parse as a US number.
        ParseNumberTestCase{true, "01178124567890", "US", "4567890", "812", "7",
                            "RU"},
        // Test for string with over 10 digits with separator characters.
        // Should give back phone number, city code, and country code. "011" is
        // US "dial out" code, which is discarded.
        ParseNumberTestCase{true, "(0111) 650-456.7890", "US", "4567890", "650",
                            "1", "US"},
        // Now try phone from Czech republic - it has 00 dial out code, 420
        // country
        // code and variable length area codes.
        ParseNumberTestCase{true, "+420 27-89.10.112", "US", "910112", "278",
                            "420", "CZ"},
        ParseNumberTestCase{false, "27-89.10.112", "US"},
        ParseNumberTestCase{true, "27-89.10.112", "CZ", "910112", "278", "",
                            "CZ"},
        ParseNumberTestCase{false, "420 57-89.10.112", "US"},
        ParseNumberTestCase{true, "420 57-89.10.112", "CZ", "910112", "578",
                            "420", "CZ"},
        // Parses vanity numbers.
        ParseNumberTestCase{true, "1-650-FLOWERS", "US", "3569377", "650", "1",
                            "US"},
        // 800 is not an area code, but the destination code. In our library
        // these
        // codes should be treated the same as area codes.
        ParseNumberTestCase{true, "1-800-FLOWERS", "US", "3569377", "800", "1",
                            "US"},
        // Don't add a country code where there was none.
        ParseNumberTestCase{true, "(08) 450 777 7777", "DE", "7777777", "8450",
                            "", "DE"}));

TEST(PhoneNumberI18NTest, ConstructPhoneNumber) {
  base::string16 number;
  EXPECT_TRUE(ConstructPhoneNumber(ASCIIToUTF16("1"),
                                   ASCIIToUTF16("650"),
                                   ASCIIToUTF16("2345678"),
                                   "US",
                                   &number));
  EXPECT_EQ(ASCIIToUTF16("1 650-234-5678"), number);
  EXPECT_TRUE(ConstructPhoneNumber(base::string16(),
                                   ASCIIToUTF16("650"),
                                   ASCIIToUTF16("2345678"),
                                   "US",
                                   &number));
  EXPECT_EQ(ASCIIToUTF16("(650) 234-5678"), number);
  EXPECT_TRUE(ConstructPhoneNumber(ASCIIToUTF16("1"),
                                   base::string16(),
                                   ASCIIToUTF16("6502345678"),
                                   "US",
                                   &number));
  EXPECT_EQ(ASCIIToUTF16("1 650-234-5678"), number);
  EXPECT_TRUE(ConstructPhoneNumber(base::string16(),
                                   base::string16(),
                                   ASCIIToUTF16("6502345678"),
                                   "US",
                                   &number));
  EXPECT_EQ(ASCIIToUTF16("(650) 234-5678"), number);

  EXPECT_FALSE(ConstructPhoneNumber(base::string16(),
                                    ASCIIToUTF16("650"),
                                    ASCIIToUTF16("234567890"),
                                    "US",
                                    &number));
  EXPECT_EQ(base::string16(), number);
  // Italian number
  EXPECT_TRUE(ConstructPhoneNumber(ASCIIToUTF16("39"),
                                   ASCIIToUTF16("347"),
                                   ASCIIToUTF16("2345678"),
                                   "IT",
                                   &number));
  EXPECT_EQ(ASCIIToUTF16("+39 347 234 5678"), number);
  EXPECT_TRUE(ConstructPhoneNumber(base::string16(),
                                   ASCIIToUTF16("347"),
                                   ASCIIToUTF16("2345678"),
                                   "IT",
                                   &number));
  EXPECT_EQ(ASCIIToUTF16("347 234 5678"), number);
  // German number.
  EXPECT_TRUE(ConstructPhoneNumber(ASCIIToUTF16("49"),
                                   ASCIIToUTF16("024"),
                                   ASCIIToUTF16("2345678901"),
                                   "DE",
                                   &number));
  EXPECT_EQ(ASCIIToUTF16("+49 2423 45678901"), number);
  EXPECT_TRUE(ConstructPhoneNumber(base::string16(),
                                   ASCIIToUTF16("024"),
                                   ASCIIToUTF16("2345678901"),
                                   "DE",
                                   &number));
  EXPECT_EQ(ASCIIToUTF16("02423 45678901"), number);
}

TEST(PhoneNumberI18NTest, PhoneNumbersMatch) {
  // Same numbers, defined country code.
  EXPECT_TRUE(PhoneNumbersMatch(ASCIIToUTF16("4158889999"),
                                ASCIIToUTF16("4158889999"),
                                "US",
                                "en-US"));
  // Same numbers, undefined country code.
  EXPECT_TRUE(PhoneNumbersMatch(ASCIIToUTF16("4158889999"),
                                ASCIIToUTF16("4158889999"),
                                std::string(),
                                "en-US"));

  // Numbers differ by country code only.
  EXPECT_TRUE(PhoneNumbersMatch(ASCIIToUTF16("14158889999"),
                                ASCIIToUTF16("4158889999"),
                                "US",
                                "en-US"));

  // Same numbers, different formats.
  EXPECT_TRUE(PhoneNumbersMatch(ASCIIToUTF16("4158889999"),
                                ASCIIToUTF16("415-888-9999"),
                                "US",
                                "en-US"));
  EXPECT_TRUE(PhoneNumbersMatch(ASCIIToUTF16("4158889999"),
                                ASCIIToUTF16("(415)888-9999"),
                                "US",
                                "en-US"));
  EXPECT_TRUE(PhoneNumbersMatch(ASCIIToUTF16("4158889999"),
                                ASCIIToUTF16("415 888 9999"),
                                "US",
                                "en-US"));
  EXPECT_TRUE(PhoneNumbersMatch(ASCIIToUTF16("4158889999"),
                                ASCIIToUTF16("415 TUV WXYZ"),
                                "US",
                                "en-US"));
  EXPECT_TRUE(PhoneNumbersMatch(ASCIIToUTF16("1(415)888-99-99"),
                                ASCIIToUTF16("+14158889999"),
                                "US",
                                "en-US"));

  // Partial matches don't count.
  EXPECT_FALSE(PhoneNumbersMatch(ASCIIToUTF16("14158889999"),
                                 ASCIIToUTF16("8889999"),
                                 "US",
                                 "en-US"));

  // Different numbers don't match.
  EXPECT_FALSE(PhoneNumbersMatch(ASCIIToUTF16("14158889999"),
                                 ASCIIToUTF16("1415888"),
                                 "US",
                                 "en-US"));
}

}  // namespace autofill
