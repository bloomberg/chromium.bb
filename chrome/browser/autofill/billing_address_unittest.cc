// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/billing_address.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(BillingAddressTest, GetPossibleFieldTypes) {
  BillingAddress address;
  address.SetInfo(AutoFillType(ADDRESS_BILLING_LINE1),
                               UTF8ToUTF16("123 Appian Way"));
  address.SetInfo(AutoFillType(ADDRESS_BILLING_LINE2), UTF8ToUTF16("Unit 6"));
  address.SetInfo(AutoFillType(ADDRESS_BILLING_APT_NUM), UTF8ToUTF16("#6"));
  address.SetInfo(AutoFillType(ADDRESS_BILLING_CITY), UTF8ToUTF16("Paris"));
  address.SetInfo(AutoFillType(ADDRESS_BILLING_STATE), UTF8ToUTF16("Texas"));
  address.SetInfo(AutoFillType(ADDRESS_BILLING_ZIP), UTF8ToUTF16("12345"));
  address.SetInfo(AutoFillType(ADDRESS_BILLING_COUNTRY), UTF8ToUTF16("USA"));

  // DCHECK on NULL |possible_types|.
  ASSERT_DEBUG_DEATH(address.GetPossibleFieldTypes(string16(), NULL), "");

  // Empty string.
  FieldTypeSet possible_types;
  address.GetPossibleFieldTypes(string16(), &possible_types);
  ASSERT_EQ(0U, possible_types.size());

  // Only use split-chars for the address.
  possible_types.clear();
  address.GetPossibleFieldTypes(UTF8ToUTF16("-,#. "), &possible_types);
  ASSERT_EQ(0U, possible_types.size());

  // ADDRESS_BILLING_LINE1 =====================================================

  // Exact match.
  possible_types.clear();
  address.GetPossibleFieldTypes(UTF8ToUTF16("123 Appian Way"), &possible_types);
  ASSERT_EQ(1U, possible_types.size());
  EXPECT_TRUE(possible_types.find(ADDRESS_BILLING_LINE1) !=
              possible_types.end());

  // Fields are mixed up.
  possible_types.clear();
  address.GetPossibleFieldTypes(UTF8ToUTF16("Way 123 Appian"), &possible_types);
  ASSERT_EQ(1U, possible_types.size());
  EXPECT_TRUE(possible_types.find(ADDRESS_BILLING_LINE1) !=
              possible_types.end());

  // The match is case-insensitive.
  possible_types.clear();
  address.GetPossibleFieldTypes(UTF8ToUTF16("123 aPPiaN wAy"), &possible_types);
  ASSERT_EQ(1U, possible_types.size());
  EXPECT_TRUE(possible_types.find(ADDRESS_BILLING_LINE1) !=
              possible_types.end());

  // Case-insensitive match with fields mixed up.  The previous test doesn't
  // do a good job of testing for case-insensitivity because the underlying
  // algorithm stops search when it matches '123'.
  possible_types.clear();
  address.GetPossibleFieldTypes(UTF8ToUTF16("wAy aPpIAn"), &possible_types);
  ASSERT_EQ(1U, possible_types.size());
  EXPECT_TRUE(possible_types.find(ADDRESS_BILLING_LINE1) !=
              possible_types.end());

  // The text is not complete.
  possible_types.clear();
  address.GetPossibleFieldTypes(UTF8ToUTF16("123 Appian"), &possible_types);
  ASSERT_EQ(1U, possible_types.size());
  EXPECT_TRUE(possible_types.find(ADDRESS_BILLING_LINE1) !=
              possible_types.end());

  // Extra text on the line.
  possible_types.clear();
  address.GetPossibleFieldTypes(UTF8ToUTF16("123 Appian Way #6"),
                                &possible_types);
  ASSERT_EQ(0U, possible_types.size());

  // Extra text in the middle
  possible_types.clear();
  address.GetPossibleFieldTypes(UTF8ToUTF16("123 Middle Appian Way"),
                                &possible_types);
  ASSERT_EQ(0U, possible_types.size());

  // Whitespace doesn't matter.
  possible_types.clear();
  address.GetPossibleFieldTypes(UTF8ToUTF16(" 123  Appian Way "),
                                &possible_types);
  ASSERT_EQ(1U, possible_types.size());
  EXPECT_TRUE(possible_types.find(ADDRESS_BILLING_LINE1) !=
              possible_types.end());

  // ADDRESS_BILLING_LINE2 =====================================================

  // Exact match.
  possible_types.clear();
  address.GetPossibleFieldTypes(UTF8ToUTF16("Unit 6"), &possible_types);
  ASSERT_EQ(1U, possible_types.size());
  EXPECT_TRUE(possible_types.find(ADDRESS_BILLING_LINE2) !=
              possible_types.end());

  // Fields are mixed up.
  possible_types.clear();
  address.GetPossibleFieldTypes(UTF8ToUTF16("6 Unit"), &possible_types);
  ASSERT_EQ(1U, possible_types.size());
  EXPECT_TRUE(possible_types.find(ADDRESS_BILLING_LINE2) !=
              possible_types.end());

  // The match is case-insensitive.
  possible_types.clear();
  address.GetPossibleFieldTypes(UTF8ToUTF16("uNiT 6"), &possible_types);
  ASSERT_EQ(1U, possible_types.size());
  EXPECT_TRUE(possible_types.find(ADDRESS_BILLING_LINE2) !=
              possible_types.end());

  // The text is not complete.
  possible_types.clear();
  address.GetPossibleFieldTypes(UTF8ToUTF16("Unit"), &possible_types);
  ASSERT_EQ(1U, possible_types.size());
  EXPECT_TRUE(possible_types.find(ADDRESS_BILLING_LINE2) !=
              possible_types.end());

  // Extra text on the line.
  possible_types.clear();
  address.GetPossibleFieldTypes(UTF8ToUTF16("Unit 6 Extra"), &possible_types);
  ASSERT_EQ(0U, possible_types.size());

  // Extra text in the middle
  possible_types.clear();
  address.GetPossibleFieldTypes(UTF8ToUTF16("Unit Middle 6"), &possible_types);
  ASSERT_EQ(0U, possible_types.size());

  // Whitespace doesn't matter.
  possible_types.clear();
  address.GetPossibleFieldTypes(UTF8ToUTF16(" Unit   6 "), &possible_types);
  ASSERT_EQ(1U, possible_types.size());
  EXPECT_TRUE(possible_types.find(ADDRESS_BILLING_LINE2) !=
              possible_types.end());

  // ADDRESS_BILLING_APT_NUM ===================================================

  // Exact match.  This matches the apartment number exactly, and also matches
  // 'Unit 6' because of the 6 and the fact that '#' is an address separator,
  // which is ignored in the search for an address line.
  possible_types.clear();
  address.GetPossibleFieldTypes(UTF8ToUTF16("#6"), &possible_types);
  ASSERT_EQ(2U, possible_types.size());
  EXPECT_TRUE(possible_types.find(ADDRESS_BILLING_APT_NUM) !=
              possible_types.end());
  EXPECT_TRUE(possible_types.find(ADDRESS_BILLING_LINE2) !=
              possible_types.end());

  // The match is case-insensitive.
  possible_types.clear();
  address.SetInfo(AutoFillType(ADDRESS_BILLING_APT_NUM), UTF8ToUTF16("Num 10"));
  address.GetPossibleFieldTypes(UTF8ToUTF16("nuM 10"), &possible_types);
  ASSERT_EQ(1U, possible_types.size());
  EXPECT_TRUE(possible_types.find(ADDRESS_BILLING_APT_NUM) !=
              possible_types.end());

  // The text is not complete.
  possible_types.clear();
  address.GetPossibleFieldTypes(UTF8ToUTF16("Num"), &possible_types);
  ASSERT_EQ(0U, possible_types.size());

  // Extra text on the line.
  possible_types.clear();
  address.GetPossibleFieldTypes(UTF8ToUTF16("Num 10 More"), &possible_types);
  ASSERT_EQ(0U, possible_types.size());

  // Extra text in the middle
  possible_types.clear();
  address.GetPossibleFieldTypes(UTF8ToUTF16("Num Middle 10"), &possible_types);
  ASSERT_EQ(0U, possible_types.size());

  // Whitespace does matter for apartment number.
  possible_types.clear();
  address.GetPossibleFieldTypes(UTF8ToUTF16(" Num 10 "),  &possible_types);
  ASSERT_EQ(0U, possible_types.size());

  // ADDRESS_BILLING_CITY ======================================================

  // Exact match.
  possible_types.clear();
  address.GetPossibleFieldTypes(UTF8ToUTF16("Paris"), &possible_types);
  ASSERT_EQ(1U, possible_types.size());
  EXPECT_TRUE(possible_types.find(ADDRESS_BILLING_CITY) !=
              possible_types.end());

  // The match is case-insensitive.
  possible_types.clear();
  address.GetPossibleFieldTypes(UTF8ToUTF16("pARiS"), &possible_types);
  ASSERT_EQ(1U, possible_types.size());
  EXPECT_TRUE(possible_types.find(ADDRESS_BILLING_CITY) !=
              possible_types.end());

  // The text is not complete.
  possible_types.clear();
  address.GetPossibleFieldTypes(UTF8ToUTF16("Par"), &possible_types);
  ASSERT_EQ(0U, possible_types.size());

  // Extra text on the line.
  possible_types.clear();
  address.GetPossibleFieldTypes(UTF8ToUTF16("Paris City"), &possible_types);
  ASSERT_EQ(0U, possible_types.size());

  // Whitespace does matter for apartment number.
  possible_types.clear();
  address.GetPossibleFieldTypes(UTF8ToUTF16(" Paris "),  &possible_types);
  ASSERT_EQ(0U, possible_types.size());

  // ADDRESS_BILLING_STATE =====================================================

  // Exact match.
  possible_types.clear();
  address.GetPossibleFieldTypes(UTF8ToUTF16("Texas"), &possible_types);
  ASSERT_EQ(1U, possible_types.size());
  EXPECT_TRUE(possible_types.find(ADDRESS_BILLING_STATE) !=
              possible_types.end());

  // The match is case-insensitive.
  possible_types.clear();
  address.GetPossibleFieldTypes(UTF8ToUTF16("tExAs"), &possible_types);
  ASSERT_EQ(1U, possible_types.size());
  EXPECT_TRUE(possible_types.find(ADDRESS_BILLING_STATE) !=
              possible_types.end());

  // The text is not complete.
  possible_types.clear();
  address.GetPossibleFieldTypes(UTF8ToUTF16("Tex"), &possible_types);
  ASSERT_EQ(0U, possible_types.size());

  // Extra text on the line.
  possible_types.clear();
  address.GetPossibleFieldTypes(UTF8ToUTF16("Texas State"), &possible_types);
  ASSERT_EQ(0U, possible_types.size());

  // Whitespace does matter for apartment number.
  possible_types.clear();
  address.GetPossibleFieldTypes(UTF8ToUTF16(" Texas "),  &possible_types);
  ASSERT_EQ(0U, possible_types.size());

  // ADDRESS_BILLING_COUNTRY ===================================================

  // Exact match.
  possible_types.clear();
  address.GetPossibleFieldTypes(UTF8ToUTF16("USA"), &possible_types);
  ASSERT_EQ(1U, possible_types.size());
  EXPECT_TRUE(possible_types.find(ADDRESS_BILLING_COUNTRY) !=
              possible_types.end());

  // The match is case-insensitive.
  possible_types.clear();
  address.GetPossibleFieldTypes(UTF8ToUTF16("uSa"), &possible_types);
  ASSERT_EQ(1U, possible_types.size());
  EXPECT_TRUE(possible_types.find(ADDRESS_BILLING_COUNTRY) !=
              possible_types.end());

  // The text is not complete.
  possible_types.clear();
  address.GetPossibleFieldTypes(UTF8ToUTF16("US"), &possible_types);
  ASSERT_EQ(0U, possible_types.size());

  // Extra text on the line.
  possible_types.clear();
  address.GetPossibleFieldTypes(UTF8ToUTF16("US Country"), &possible_types);
  ASSERT_EQ(0U, possible_types.size());

  // Whitespace does matter for apartment number.
  possible_types.clear();
  address.GetPossibleFieldTypes(UTF8ToUTF16(" US "),  &possible_types);
  ASSERT_EQ(0U, possible_types.size());

  // ADDRESS_BILLING_ZIP =======================================================

  // Exact match.
  possible_types.clear();
  address.GetPossibleFieldTypes(UTF8ToUTF16("12345"), &possible_types);
  ASSERT_EQ(1U, possible_types.size());
  EXPECT_TRUE(possible_types.find(ADDRESS_BILLING_ZIP) !=
              possible_types.end());

  // The text is not complete.
  possible_types.clear();
  address.GetPossibleFieldTypes(UTF8ToUTF16("1234"), &possible_types);
  ASSERT_EQ(0U, possible_types.size());

  // Extra text on the line.
  possible_types.clear();
  address.GetPossibleFieldTypes(UTF8ToUTF16("12345 678"), &possible_types);
  ASSERT_EQ(0U, possible_types.size());

  // Whitespace does matter for apartment number.
  possible_types.clear();
  address.GetPossibleFieldTypes(UTF8ToUTF16(" 12345 "),  &possible_types);
  ASSERT_EQ(0U, possible_types.size());

  // Misc =======================================================

  // More than one type can match.
  address.SetInfo(AutoFillType(ADDRESS_BILLING_LINE1), UTF8ToUTF16("Same"));
  address.SetInfo(AutoFillType(ADDRESS_BILLING_LINE2), UTF8ToUTF16("Same"));
  address.SetInfo(AutoFillType(ADDRESS_BILLING_APT_NUM), UTF8ToUTF16("Same"));
  possible_types.clear();
  address.GetPossibleFieldTypes(UTF8ToUTF16("Same"), &possible_types);
  ASSERT_EQ(3U, possible_types.size());
  EXPECT_TRUE(possible_types.find(ADDRESS_BILLING_LINE1) !=
              possible_types.end());
  EXPECT_TRUE(possible_types.find(ADDRESS_BILLING_LINE2) !=
              possible_types.end());
  EXPECT_TRUE(possible_types.find(ADDRESS_BILLING_APT_NUM) !=
              possible_types.end());
}
