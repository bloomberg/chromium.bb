// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/address.h"
#include "chrome/browser/autofill/billing_address.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class BillingAddressTest : public testing::Test {
 public:
  BillingAddressTest() {
    address_.SetInfo(AutoFillType(ADDRESS_BILLING_LINE1),
                     ASCIIToUTF16("123 Appian Way"));
    address_.SetInfo(AutoFillType(ADDRESS_BILLING_LINE2),
                     ASCIIToUTF16("Unit 6"));
    address_.SetInfo(AutoFillType(ADDRESS_BILLING_APT_NUM), ASCIIToUTF16("#6"));
    address_.SetInfo(AutoFillType(ADDRESS_BILLING_CITY), ASCIIToUTF16("Paris"));
    address_.SetInfo(AutoFillType(ADDRESS_BILLING_STATE),
                     ASCIIToUTF16("Texas"));
    address_.SetInfo(AutoFillType(ADDRESS_BILLING_ZIP), ASCIIToUTF16("12345"));
    address_.SetInfo(AutoFillType(ADDRESS_BILLING_COUNTRY),
                     ASCIIToUTF16("USA"));
  }

 protected:
  BillingAddress address_;

  DISALLOW_COPY_AND_ASSIGN(BillingAddressTest);
};

TEST_F(BillingAddressTest, GetPossibleFieldTypes) {
  // Empty string.
  FieldTypeSet possible_types;
  address_.GetPossibleFieldTypes(string16(), &possible_types);
  ASSERT_EQ(0U, possible_types.size());

  // Only use split-chars for the address_.
  possible_types.clear();
  address_.GetPossibleFieldTypes(ASCIIToUTF16("-,#. "), &possible_types);
  ASSERT_EQ(0U, possible_types.size());

  // ADDRESS_BILLING_LINE1 =====================================================

  // Exact match.
  possible_types.clear();
  address_.GetPossibleFieldTypes(ASCIIToUTF16("123 Appian Way"),
                                 &possible_types);
  ASSERT_EQ(1U, possible_types.size());
  EXPECT_TRUE(possible_types.find(ADDRESS_BILLING_LINE1) !=
              possible_types.end());

  // Fields are mixed up.
  possible_types.clear();
  address_.GetPossibleFieldTypes(ASCIIToUTF16("Way 123 Appian"),
                                 &possible_types);
  ASSERT_EQ(1U, possible_types.size());
  EXPECT_TRUE(possible_types.find(ADDRESS_BILLING_LINE1) !=
              possible_types.end());

  // The match is case-insensitive.
  possible_types.clear();
  address_.GetPossibleFieldTypes(ASCIIToUTF16("123 aPPiaN wAy"),
                                 &possible_types);
  ASSERT_EQ(1U, possible_types.size());
  EXPECT_TRUE(possible_types.find(ADDRESS_BILLING_LINE1) !=
              possible_types.end());

  // Case-insensitive match with fields mixed up.  The previous test doesn't
  // do a good job of testing for case-insensitivity because the underlying
  // algorithm stops search when it matches '123'.
  possible_types.clear();
  address_.GetPossibleFieldTypes(ASCIIToUTF16("wAy aPpIAn"), &possible_types);
  ASSERT_EQ(1U, possible_types.size());
  EXPECT_TRUE(possible_types.find(ADDRESS_BILLING_LINE1) !=
              possible_types.end());

  // The text is not complete.
  possible_types.clear();
  address_.GetPossibleFieldTypes(ASCIIToUTF16("123 Appian"), &possible_types);
  ASSERT_EQ(1U, possible_types.size());
  EXPECT_TRUE(possible_types.find(ADDRESS_BILLING_LINE1) !=
              possible_types.end());

  // Extra text on the line.
  possible_types.clear();
  address_.GetPossibleFieldTypes(ASCIIToUTF16("123 Appian Way #6"),
                                &possible_types);
  ASSERT_EQ(0U, possible_types.size());

  // Extra text in the middle
  possible_types.clear();
  address_.GetPossibleFieldTypes(ASCIIToUTF16("123 Middle Appian Way"),
                                &possible_types);
  ASSERT_EQ(0U, possible_types.size());

  // Whitespace doesn't matter.
  possible_types.clear();
  address_.GetPossibleFieldTypes(ASCIIToUTF16(" 123  Appian Way "),
                                &possible_types);
  ASSERT_EQ(1U, possible_types.size());
  EXPECT_TRUE(possible_types.find(ADDRESS_BILLING_LINE1) !=
              possible_types.end());

  // Address split characters don't matter.
  possible_types.clear();
  address_.GetPossibleFieldTypes(ASCIIToUTF16("-123, #Appian.Way"),
                                &possible_types);
  ASSERT_EQ(1U, possible_types.size());
  EXPECT_TRUE(possible_types.find(ADDRESS_BILLING_LINE1) !=
              possible_types.end());

  // ADDRESS_BILLING_LINE2 =====================================================

  // Exact match.
  possible_types.clear();
  address_.GetPossibleFieldTypes(ASCIIToUTF16("Unit 6"), &possible_types);
  ASSERT_EQ(1U, possible_types.size());
  EXPECT_TRUE(possible_types.find(ADDRESS_BILLING_LINE2) !=
              possible_types.end());

  // Fields are mixed up.
  possible_types.clear();
  address_.GetPossibleFieldTypes(ASCIIToUTF16("6 Unit"), &possible_types);
  ASSERT_EQ(1U, possible_types.size());
  EXPECT_TRUE(possible_types.find(ADDRESS_BILLING_LINE2) !=
              possible_types.end());

  // The match is case-insensitive.
  possible_types.clear();
  address_.GetPossibleFieldTypes(ASCIIToUTF16("uNiT 6"), &possible_types);
  ASSERT_EQ(1U, possible_types.size());
  EXPECT_TRUE(possible_types.find(ADDRESS_BILLING_LINE2) !=
              possible_types.end());

  // The text is not complete.
  possible_types.clear();
  address_.GetPossibleFieldTypes(ASCIIToUTF16("Unit"), &possible_types);
  ASSERT_EQ(1U, possible_types.size());
  EXPECT_TRUE(possible_types.find(ADDRESS_BILLING_LINE2) !=
              possible_types.end());

  // Extra text on the line.
  possible_types.clear();
  address_.GetPossibleFieldTypes(ASCIIToUTF16("Unit 6 Extra"), &possible_types);
  ASSERT_EQ(0U, possible_types.size());

  // Extra text in the middle
  possible_types.clear();
  address_.GetPossibleFieldTypes(ASCIIToUTF16("Unit Middle 6"),
                                 &possible_types);
  ASSERT_EQ(0U, possible_types.size());

  // Whitespace doesn't matter.
  possible_types.clear();
  address_.GetPossibleFieldTypes(ASCIIToUTF16(" Unit   6 "), &possible_types);
  ASSERT_EQ(1U, possible_types.size());
  EXPECT_TRUE(possible_types.find(ADDRESS_BILLING_LINE2) !=
              possible_types.end());

  // Address split characters don't matter.
  possible_types.clear();
  address_.GetPossibleFieldTypes(ASCIIToUTF16("-#Unit, .6"),
                                &possible_types);
  ASSERT_EQ(1U, possible_types.size());
  EXPECT_TRUE(possible_types.find(ADDRESS_BILLING_LINE2) !=
              possible_types.end());

  // ADDRESS_BILLING_APT_NUM ===================================================

  // Exact match.  This matches the apartment number exactly, and also matches
  // 'Unit 6' because of the 6 and the fact that '#' is an address separator,
  // which is ignored in the search for an address line.
  possible_types.clear();
  address_.GetPossibleFieldTypes(ASCIIToUTF16("#6"), &possible_types);
  ASSERT_EQ(2U, possible_types.size());
  EXPECT_TRUE(possible_types.find(ADDRESS_BILLING_APT_NUM) !=
              possible_types.end());
  EXPECT_TRUE(possible_types.find(ADDRESS_BILLING_LINE2) !=
              possible_types.end());

  // The match is case-insensitive.
  possible_types.clear();
  address_.SetInfo(AutoFillType(ADDRESS_BILLING_APT_NUM),
                                ASCIIToUTF16("Num 10"));
  address_.GetPossibleFieldTypes(ASCIIToUTF16("nuM 10"), &possible_types);
  ASSERT_EQ(1U, possible_types.size());
  EXPECT_TRUE(possible_types.find(ADDRESS_BILLING_APT_NUM) !=
              possible_types.end());

  // The text is not complete.
  possible_types.clear();
  address_.GetPossibleFieldTypes(ASCIIToUTF16("Num"), &possible_types);
  ASSERT_EQ(0U, possible_types.size());

  // Extra text on the line.
  possible_types.clear();
  address_.GetPossibleFieldTypes(ASCIIToUTF16("Num 10 More"), &possible_types);
  ASSERT_EQ(0U, possible_types.size());

  // Extra text in the middle
  possible_types.clear();
  address_.GetPossibleFieldTypes(ASCIIToUTF16("Num Middle 10"),
                                 &possible_types);
  ASSERT_EQ(0U, possible_types.size());

  // Whitespace does matter for apartment number.
  possible_types.clear();
  address_.GetPossibleFieldTypes(ASCIIToUTF16(" Num 10 "),  &possible_types);
  ASSERT_EQ(0U, possible_types.size());

  // ADDRESS_BILLING_CITY ======================================================

  // Exact match.
  possible_types.clear();
  address_.GetPossibleFieldTypes(ASCIIToUTF16("Paris"), &possible_types);
  ASSERT_EQ(1U, possible_types.size());
  EXPECT_TRUE(possible_types.find(ADDRESS_BILLING_CITY) !=
              possible_types.end());

  // The match is case-insensitive.
  possible_types.clear();
  address_.GetPossibleFieldTypes(ASCIIToUTF16("pARiS"), &possible_types);
  ASSERT_EQ(1U, possible_types.size());
  EXPECT_TRUE(possible_types.find(ADDRESS_BILLING_CITY) !=
              possible_types.end());

  // The text is not complete.
  possible_types.clear();
  address_.GetPossibleFieldTypes(ASCIIToUTF16("Par"), &possible_types);
  ASSERT_EQ(0U, possible_types.size());

  // Extra text on the line.
  possible_types.clear();
  address_.GetPossibleFieldTypes(ASCIIToUTF16("Paris City"), &possible_types);
  ASSERT_EQ(0U, possible_types.size());

  // Whitespace does matter for apartment number.
  possible_types.clear();
  address_.GetPossibleFieldTypes(ASCIIToUTF16(" Paris "),  &possible_types);
  ASSERT_EQ(0U, possible_types.size());

  // ADDRESS_BILLING_STATE =====================================================

  // Exact match.
  possible_types.clear();
  address_.GetPossibleFieldTypes(ASCIIToUTF16("Texas"), &possible_types);
  ASSERT_EQ(1U, possible_types.size());
  EXPECT_TRUE(possible_types.find(ADDRESS_BILLING_STATE) !=
              possible_types.end());

  // The match is case-insensitive.
  possible_types.clear();
  address_.GetPossibleFieldTypes(ASCIIToUTF16("tExAs"), &possible_types);
  ASSERT_EQ(1U, possible_types.size());
  EXPECT_TRUE(possible_types.find(ADDRESS_BILLING_STATE) !=
              possible_types.end());

  // The text is not complete.
  possible_types.clear();
  address_.GetPossibleFieldTypes(ASCIIToUTF16("Tex"), &possible_types);
  ASSERT_EQ(0U, possible_types.size());

  // Extra text on the line.
  possible_types.clear();
  address_.GetPossibleFieldTypes(ASCIIToUTF16("Texas State"), &possible_types);
  ASSERT_EQ(0U, possible_types.size());

  // Whitespace does matter for apartment number.
  possible_types.clear();
  address_.GetPossibleFieldTypes(ASCIIToUTF16(" Texas "),  &possible_types);
  ASSERT_EQ(0U, possible_types.size());

  // ADDRESS_BILLING_COUNTRY ===================================================

  // Exact match.
  possible_types.clear();
  address_.GetPossibleFieldTypes(ASCIIToUTF16("USA"), &possible_types);
  ASSERT_EQ(1U, possible_types.size());
  EXPECT_TRUE(possible_types.find(ADDRESS_BILLING_COUNTRY) !=
              possible_types.end());

  // The match is case-insensitive.
  possible_types.clear();
  address_.GetPossibleFieldTypes(ASCIIToUTF16("uSa"), &possible_types);
  ASSERT_EQ(1U, possible_types.size());
  EXPECT_TRUE(possible_types.find(ADDRESS_BILLING_COUNTRY) !=
              possible_types.end());

  // The text is not complete.
  possible_types.clear();
  address_.GetPossibleFieldTypes(ASCIIToUTF16("US"), &possible_types);
  ASSERT_EQ(0U, possible_types.size());

  // Extra text on the line.
  possible_types.clear();
  address_.GetPossibleFieldTypes(ASCIIToUTF16("US Country"), &possible_types);
  ASSERT_EQ(0U, possible_types.size());

  // Whitespace does matter for apartment number.
  possible_types.clear();
  address_.GetPossibleFieldTypes(ASCIIToUTF16(" US "),  &possible_types);
  ASSERT_EQ(0U, possible_types.size());

  // ADDRESS_BILLING_ZIP =======================================================

  // Exact match.
  possible_types.clear();
  address_.GetPossibleFieldTypes(ASCIIToUTF16("12345"), &possible_types);
  ASSERT_EQ(1U, possible_types.size());
  EXPECT_TRUE(possible_types.find(ADDRESS_BILLING_ZIP) !=
              possible_types.end());

  // The text is not complete.
  possible_types.clear();
  address_.GetPossibleFieldTypes(ASCIIToUTF16("1234"), &possible_types);
  ASSERT_EQ(0U, possible_types.size());

  // Extra text on the line.
  possible_types.clear();
  address_.GetPossibleFieldTypes(ASCIIToUTF16("12345 678"), &possible_types);
  ASSERT_EQ(0U, possible_types.size());

  // Whitespace does matter for apartment number.
  possible_types.clear();
  address_.GetPossibleFieldTypes(ASCIIToUTF16(" 12345 "),  &possible_types);
  ASSERT_EQ(0U, possible_types.size());

  // Misc ======================================================================

  // More than one type can match.
  address_.SetInfo(AutoFillType(ADDRESS_BILLING_LINE1), ASCIIToUTF16("Same"));
  address_.SetInfo(AutoFillType(ADDRESS_BILLING_LINE2), ASCIIToUTF16("Same"));
  address_.SetInfo(AutoFillType(ADDRESS_BILLING_APT_NUM), ASCIIToUTF16("Same"));
  possible_types.clear();
  address_.GetPossibleFieldTypes(ASCIIToUTF16("Same"), &possible_types);
  ASSERT_EQ(3U, possible_types.size());
  EXPECT_TRUE(possible_types.find(ADDRESS_BILLING_LINE1) !=
              possible_types.end());
  EXPECT_TRUE(possible_types.find(ADDRESS_BILLING_LINE2) !=
              possible_types.end());
  EXPECT_TRUE(possible_types.find(ADDRESS_BILLING_APT_NUM) !=
              possible_types.end());

  // LINE1 is empty.
  address_.SetInfo(AutoFillType(ADDRESS_BILLING_LINE1), string16());
  possible_types.clear();
  address_.GetPossibleFieldTypes(string16(), &possible_types);
  ASSERT_EQ(0U, possible_types.size());
}

TEST_F(BillingAddressTest, FindInfoMatches) {
  // ADDRESS_BILLING_LINE1 =====================================================

  // Match the beginning of the string.
  std::vector<string16> matches;
  address_.FindInfoMatches(
      AutoFillType(ADDRESS_BILLING_LINE1), ASCIIToUTF16("123"), &matches);
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(ASCIIToUTF16("123 Appian Way"), matches[0]);

  // Search has too many characters.
  matches.clear();
  address_.FindInfoMatches(
      AutoFillType(ADDRESS_BILLING_LINE1), ASCIIToUTF16("123 B"), &matches);
  ASSERT_EQ(0U, matches.size());

  // Whitespace at the beginning of the search.
  matches.clear();
  address_.FindInfoMatches(
      AutoFillType(ADDRESS_BILLING_LINE1), ASCIIToUTF16(" 123"), &matches);
  ASSERT_EQ(0U, matches.size());

  // Search includes the entire match, but adds extra characters.
  matches.clear();
  address_.FindInfoMatches(AutoFillType(ADDRESS_BILLING_LINE1),
                          ASCIIToUTF16("123 Appian Way B"),
                          &matches);
  ASSERT_EQ(0U, matches.size());

  // Matching is case-insensitive.
  matches.clear();
  address_.FindInfoMatches(AutoFillType(ADDRESS_BILLING_LINE1),
                          ASCIIToUTF16("123 aPpiAN wAy"),
                          &matches);
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(ASCIIToUTF16("123 Appian Way"), matches[0]);

  // Search is empty.
  matches.clear();
  address_.FindInfoMatches(
      AutoFillType(ADDRESS_BILLING_LINE1), string16(), &matches);
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(ASCIIToUTF16("123 Appian Way"), matches[0]);

  // ADDRESS_BILLING_LINE2 =====================================================

  // Match the beginning of the string.
  matches.clear();
  address_.FindInfoMatches(
      AutoFillType(ADDRESS_BILLING_LINE2), ASCIIToUTF16("Unit"), &matches);
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(ASCIIToUTF16("Unit 6"), matches[0]);

  // Search has too many characters.
  matches.clear();
  address_.FindInfoMatches(
      AutoFillType(ADDRESS_BILLING_LINE2), ASCIIToUTF16("Unita"), &matches);
  ASSERT_EQ(0U, matches.size());

  // Whitespace at the beginning of the search.
  matches.clear();
  address_.FindInfoMatches(
      AutoFillType(ADDRESS_BILLING_LINE2), ASCIIToUTF16(" Unit"), &matches);
  ASSERT_EQ(0U, matches.size());

  // Search includes the entire match, but adds extra characters.
  matches.clear();
  address_.FindInfoMatches(
      AutoFillType(ADDRESS_BILLING_LINE2), ASCIIToUTF16("Unit 6B"), &matches);
  ASSERT_EQ(0U, matches.size());

  // Matching is case-insensitive.
  matches.clear();
  address_.FindInfoMatches(
      AutoFillType(ADDRESS_BILLING_LINE2), ASCIIToUTF16("uNiT 6"), &matches);
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(ASCIIToUTF16("Unit 6"), matches[0]);

  // Search is empty.
  matches.clear();
  address_.FindInfoMatches(
      AutoFillType(ADDRESS_BILLING_LINE2), string16(), &matches);
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(ASCIIToUTF16("Unit 6"), matches[0]);

  // ADDRESS_BILLING_APT_NUM ===================================================

  // Match the beginning of the string.
  matches.clear();
  address_.FindInfoMatches(
      AutoFillType(ADDRESS_BILLING_APT_NUM), ASCIIToUTF16("#"), &matches);
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(ASCIIToUTF16("#6"), matches[0]);

  // Search has too many characters.
  matches.clear();
  address_.FindInfoMatches(
      AutoFillType(ADDRESS_BILLING_APT_NUM), ASCIIToUTF16("#a"), &matches);
  ASSERT_EQ(0U, matches.size());

  // Whitespace at the beginning of the search.
  matches.clear();
  address_.FindInfoMatches(
      AutoFillType(ADDRESS_BILLING_APT_NUM), ASCIIToUTF16(" #"), &matches);
  ASSERT_EQ(0U, matches.size());

  // Search includes the entire match, but adds extra characters.
  matches.clear();
  address_.FindInfoMatches(
      AutoFillType(ADDRESS_BILLING_APT_NUM), ASCIIToUTF16("#6B"), &matches);
  ASSERT_EQ(0U, matches.size());

  // Matching is case-insensitive.
  matches.clear();
  address_.SetInfo(AutoFillType(ADDRESS_BILLING_APT_NUM), ASCIIToUTF16("6B"));
  address_.FindInfoMatches(
      AutoFillType(ADDRESS_BILLING_APT_NUM), ASCIIToUTF16("6b"), &matches);
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(ASCIIToUTF16("6B"), matches[0]);

  // Search is empty.
  matches.clear();
  address_.FindInfoMatches(
      AutoFillType(ADDRESS_BILLING_APT_NUM), string16(), &matches);
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(ASCIIToUTF16("6B"), matches[0]);

  // ADDRESS_BILLING_CITY ======================================================

  // Match the beginning of the string.
  matches.clear();
  address_.FindInfoMatches(
      AutoFillType(ADDRESS_BILLING_CITY), ASCIIToUTF16("Par"), &matches);
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(ASCIIToUTF16("Paris"), matches[0]);

  // Search has too many characters.
  matches.clear();
  address_.FindInfoMatches(
      AutoFillType(ADDRESS_BILLING_CITY), ASCIIToUTF16("ParA"), &matches);
  ASSERT_EQ(0U, matches.size());

  // Whitespace at the beginning of the search.
  matches.clear();
  address_.FindInfoMatches(
      AutoFillType(ADDRESS_BILLING_CITY), ASCIIToUTF16(" Paris"), &matches);
  ASSERT_EQ(0U, matches.size());

  // Search includes the entire match, but adds extra characters.
  matches.clear();
  address_.FindInfoMatches(
      AutoFillType(ADDRESS_BILLING_CITY), ASCIIToUTF16("ParisB"), &matches);
  ASSERT_EQ(0U, matches.size());

  // Matching is case-insensitive.
  matches.clear();
  address_.FindInfoMatches(
      AutoFillType(ADDRESS_BILLING_CITY), ASCIIToUTF16("PArIs"), &matches);
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(ASCIIToUTF16("Paris"), matches[0]);

  // Search is empty.
  matches.clear();
  address_.FindInfoMatches(
      AutoFillType(ADDRESS_BILLING_CITY), string16(), &matches);
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(ASCIIToUTF16("Paris"), matches[0]);

  // ADDRESS_BILLING_STATE =====================================================

  // Match the beginning of the string.
  matches.clear();
  address_.FindInfoMatches(
      AutoFillType(ADDRESS_BILLING_STATE), ASCIIToUTF16("Tex"), &matches);
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(ASCIIToUTF16("Texas"), matches[0]);

  // Search has too many characters.
  matches.clear();
  address_.FindInfoMatches(
      AutoFillType(ADDRESS_BILLING_STATE), ASCIIToUTF16("TexC"), &matches);
  ASSERT_EQ(0U, matches.size());

  // Whitespace at the beginning of the search.
  matches.clear();
  address_.FindInfoMatches(
      AutoFillType(ADDRESS_BILLING_STATE), ASCIIToUTF16(" Texas"), &matches);
  ASSERT_EQ(0U, matches.size());

  // Search includes the entire match, but adds extra characters.
  matches.clear();
  address_.FindInfoMatches(
      AutoFillType(ADDRESS_BILLING_STATE), ASCIIToUTF16("TexasB"), &matches);
  ASSERT_EQ(0U, matches.size());

  // Matching is case-insensitive.
  matches.clear();
  address_.FindInfoMatches(
      AutoFillType(ADDRESS_BILLING_STATE), ASCIIToUTF16("TeXaS"), &matches);
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(ASCIIToUTF16("Texas"), matches[0]);

  // Search is empty.
  matches.clear();
  address_.FindInfoMatches(
      AutoFillType(ADDRESS_BILLING_STATE), string16(), &matches);
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(ASCIIToUTF16("Texas"), matches[0]);

  // ADDRESS_BILLING_ZIP =======================================================

  // Match the beginning of the string.
  matches.clear();
  address_.FindInfoMatches(
      AutoFillType(ADDRESS_BILLING_ZIP), ASCIIToUTF16("123"), &matches);
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(ASCIIToUTF16("12345"), matches[0]);

  // Search has too many characters.
  matches.clear();
  address_.FindInfoMatches(
      AutoFillType(ADDRESS_BILLING_ZIP), ASCIIToUTF16("123a"), &matches);
  ASSERT_EQ(0U, matches.size());

  // Whitespace at the beginning of the search.
  matches.clear();
  address_.FindInfoMatches(
      AutoFillType(ADDRESS_BILLING_ZIP), ASCIIToUTF16(" 123"), &matches);
  ASSERT_EQ(0U, matches.size());

  // Search includes the entire match, but adds extra characters.
  matches.clear();
  address_.FindInfoMatches(
      AutoFillType(ADDRESS_BILLING_ZIP), ASCIIToUTF16("123456"), &matches);
  ASSERT_EQ(0U, matches.size());

  // Matching is case-sensitive because we should only have numbers in the zip.
  matches.clear();
  address_.SetInfo(AutoFillType(ADDRESS_BILLING_ZIP), ASCIIToUTF16("12345A"));
  address_.FindInfoMatches(
      AutoFillType(ADDRESS_BILLING_ZIP), ASCIIToUTF16("12345a"), &matches);
  ASSERT_EQ(0U, matches.size());

  // Reset the zip code.
  address_.SetInfo(AutoFillType(ADDRESS_BILLING_ZIP), ASCIIToUTF16("12345"));

  // Search is empty.
  matches.clear();
  address_.FindInfoMatches(
      AutoFillType(ADDRESS_BILLING_ZIP), string16(), &matches);
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(ASCIIToUTF16("12345"), matches[0]);

  // ADDRESS_BILLING_COUNTRY ===================================================

  // Match the beginning of the string.
  matches.clear();
  address_.FindInfoMatches(
      AutoFillType(ADDRESS_BILLING_COUNTRY), ASCIIToUTF16("US"), &matches);
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(ASCIIToUTF16("USA"), matches[0]);

  // Search has too many characters.
  matches.clear();
  address_.FindInfoMatches(
      AutoFillType(ADDRESS_BILLING_COUNTRY), ASCIIToUTF16("USb"), &matches);
  ASSERT_EQ(0U, matches.size());

  // Whitespace at the beginning of the search.
  matches.clear();
  address_.FindInfoMatches(
      AutoFillType(ADDRESS_BILLING_COUNTRY), ASCIIToUTF16(" US"), &matches);
  ASSERT_EQ(0U, matches.size());

  // Search includes the entire match, but adds extra characters.
  matches.clear();
  address_.FindInfoMatches(
      AutoFillType(ADDRESS_BILLING_COUNTRY), ASCIIToUTF16("USAB"), &matches);
  ASSERT_EQ(0U, matches.size());

  // Matching is case-insensitive.
  matches.clear();
  address_.FindInfoMatches(
      AutoFillType(ADDRESS_BILLING_COUNTRY), ASCIIToUTF16("uSa"), &matches);
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(ASCIIToUTF16("USA"), matches[0]);

  // Search is empty.
  matches.clear();
  address_.FindInfoMatches(
      AutoFillType(ADDRESS_BILLING_COUNTRY), string16(), &matches);
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(ASCIIToUTF16("USA"), matches[0]);

  // Misc ======================================================================

  // |type| is not handled by address_.
  matches.clear();
  address_.FindInfoMatches(
      AutoFillType(NAME_FIRST), ASCIIToUTF16("USA"), &matches);
  ASSERT_EQ(0U, matches.size());

  // |type| is UNKNOWN_TYPE.
  matches.clear();
  address_.FindInfoMatches(
      AutoFillType(UNKNOWN_TYPE), ASCIIToUTF16("123"), &matches);
  ASSERT_EQ(2U, matches.size());
  EXPECT_EQ(ASCIIToUTF16("123 Appian Way"), matches[0]);
  EXPECT_EQ(ASCIIToUTF16("12345"), matches[1]);

  // |type| is UNKNOWN_TYPE.  Exclude zip because of extra space.
  matches.clear();
  address_.FindInfoMatches(
      AutoFillType(UNKNOWN_TYPE), ASCIIToUTF16("123 "), &matches);
  ASSERT_EQ(1U, matches.size());
  EXPECT_EQ(ASCIIToUTF16("123 Appian Way"), matches[0]);

  // |type| is UNKNOWN_TYPE.  Search is empty.
  matches.clear();
  address_.FindInfoMatches(
      AutoFillType(UNKNOWN_TYPE), string16(), &matches);
  ASSERT_EQ(7U, matches.size());
  EXPECT_EQ(ASCIIToUTF16("123 Appian Way"), matches[0]);
  EXPECT_EQ(ASCIIToUTF16("Unit 6"), matches[1]);
  EXPECT_EQ(ASCIIToUTF16("6B"), matches[2]);
  EXPECT_EQ(ASCIIToUTF16("Paris"), matches[3]);
  EXPECT_EQ(ASCIIToUTF16("Texas"), matches[4]);
  EXPECT_EQ(ASCIIToUTF16("12345"), matches[5]);
  EXPECT_EQ(ASCIIToUTF16("USA"), matches[6]);
}

TEST_F(BillingAddressTest, GetFieldText) {
  // Get the field text.
  string16 text;
  text = address_.GetFieldText(AutoFillType(ADDRESS_BILLING_LINE1));
  EXPECT_EQ(ASCIIToUTF16("123 Appian Way"), text);
  text = address_.GetFieldText(AutoFillType(ADDRESS_BILLING_LINE2));
  EXPECT_EQ(ASCIIToUTF16("Unit 6"), text);
  text = address_.GetFieldText(AutoFillType(ADDRESS_BILLING_APT_NUM));
  EXPECT_EQ(ASCIIToUTF16("#6"), text);
  text = address_.GetFieldText(AutoFillType(ADDRESS_BILLING_CITY));
  EXPECT_EQ(ASCIIToUTF16("Paris"), text);
  text = address_.GetFieldText(AutoFillType(ADDRESS_BILLING_STATE));
  EXPECT_EQ(ASCIIToUTF16("Texas"), text);
  text = address_.GetFieldText(AutoFillType(ADDRESS_BILLING_ZIP));
  EXPECT_EQ(ASCIIToUTF16("12345"), text);
  text = address_.GetFieldText(AutoFillType(ADDRESS_BILLING_COUNTRY));
  EXPECT_EQ(ASCIIToUTF16("USA"), text);

  // |type| is not supported by Billingaddress_.
  text = address_.GetFieldText(AutoFillType(NAME_FIRST));
  EXPECT_EQ(string16(), text);
}

TEST_F(BillingAddressTest, Clear) {
  // Clear the info.
  string16 text;
  address_.Clear();
  text = address_.GetFieldText(AutoFillType(ADDRESS_BILLING_LINE1));
  EXPECT_EQ(string16(), text);
  text = address_.GetFieldText(AutoFillType(ADDRESS_BILLING_LINE2));
  EXPECT_EQ(string16(), text);
  text = address_.GetFieldText(AutoFillType(ADDRESS_BILLING_APT_NUM));
  EXPECT_EQ(string16(), text);
  text = address_.GetFieldText(AutoFillType(ADDRESS_BILLING_CITY));
  EXPECT_EQ(string16(), text);
  text = address_.GetFieldText(AutoFillType(ADDRESS_BILLING_STATE));
  EXPECT_EQ(string16(), text);
  text = address_.GetFieldText(AutoFillType(ADDRESS_BILLING_ZIP));
  EXPECT_EQ(string16(), text);
  text = address_.GetFieldText(AutoFillType(ADDRESS_BILLING_COUNTRY));
  EXPECT_EQ(string16(), text);
}

TEST_F(BillingAddressTest, AddressClone) {
  // Clone the info.
  string16 text;
  BillingAddress clone;
  Address* clone_ptr = &clone;
  clone_ptr->Clone(address_);
  text = clone.GetFieldText(AutoFillType(ADDRESS_BILLING_LINE1));
  EXPECT_EQ(ASCIIToUTF16("123 Appian Way"), text);
  text = clone.GetFieldText(AutoFillType(ADDRESS_BILLING_LINE2));
  EXPECT_EQ(ASCIIToUTF16("Unit 6"), text);
  text = clone.GetFieldText(AutoFillType(ADDRESS_BILLING_APT_NUM));
  EXPECT_EQ(ASCIIToUTF16("#6"), text);
  text = clone.GetFieldText(AutoFillType(ADDRESS_BILLING_CITY));
  EXPECT_EQ(ASCIIToUTF16("Paris"), text);
  text = clone.GetFieldText(AutoFillType(ADDRESS_BILLING_STATE));
  EXPECT_EQ(ASCIIToUTF16("Texas"), text);
  text = clone.GetFieldText(AutoFillType(ADDRESS_BILLING_ZIP));
  EXPECT_EQ(ASCIIToUTF16("12345"), text);
  text = clone.GetFieldText(AutoFillType(ADDRESS_BILLING_COUNTRY));
  EXPECT_EQ(ASCIIToUTF16("USA"), text);

  // Verify that the clone is a copy and not a reference.
  address_.SetInfo(AutoFillType(ADDRESS_BILLING_LINE1),
                   ASCIIToUTF16("654 Bowling Terrace"));
  text = clone.GetFieldText(AutoFillType(ADDRESS_BILLING_LINE1));
  EXPECT_EQ(ASCIIToUTF16("123 Appian Way"), text);
}

TEST_F(BillingAddressTest, BillingAddressClone) {
  // Clone the info.
  string16 text;
  scoped_ptr<FormGroup> clone(address_.Clone());
  ASSERT_NE(static_cast<FormGroup*>(NULL), clone.get());
  text = clone->GetFieldText(AutoFillType(ADDRESS_BILLING_LINE1));
  EXPECT_EQ(ASCIIToUTF16("123 Appian Way"), text);
  text = clone->GetFieldText(AutoFillType(ADDRESS_BILLING_LINE2));
  EXPECT_EQ(ASCIIToUTF16("Unit 6"), text);
  text = clone->GetFieldText(AutoFillType(ADDRESS_BILLING_APT_NUM));
  EXPECT_EQ(ASCIIToUTF16("#6"), text);
  text = clone->GetFieldText(AutoFillType(ADDRESS_BILLING_CITY));
  EXPECT_EQ(ASCIIToUTF16("Paris"), text);
  text = clone->GetFieldText(AutoFillType(ADDRESS_BILLING_STATE));
  EXPECT_EQ(ASCIIToUTF16("Texas"), text);
  text = clone->GetFieldText(AutoFillType(ADDRESS_BILLING_ZIP));
  EXPECT_EQ(ASCIIToUTF16("12345"), text);
  text = clone->GetFieldText(AutoFillType(ADDRESS_BILLING_COUNTRY));
  EXPECT_EQ(ASCIIToUTF16("USA"), text);

  // Verify that the clone is a copy and not a reference.
  address_.SetInfo(AutoFillType(ADDRESS_BILLING_LINE1),
                   ASCIIToUTF16("654 Bowling Terrace"));
  text = clone->GetFieldText(AutoFillType(ADDRESS_BILLING_LINE1));
  EXPECT_EQ(ASCIIToUTF16("123 Appian Way"), text);
}

}  // namespace
