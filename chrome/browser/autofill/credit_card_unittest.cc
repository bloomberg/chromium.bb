// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_common_test.h"
#include "chrome/browser/autofill/credit_card.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// From https://www.paypalobjects.com/en_US/vhelp/paypalmanager_help/credit_card_numbers.htm
const char* const kValidNumbers[] = {
  "378282246310005",
  "3714 4963 5398 431",
  "3787-3449-3671-000",
  "5610591081018250",
  "3056 9309 0259 04",
  "3852-0000-0232-37",
  "6011111111111117",
  "6011 0009 9013 9424",
  "3530-1113-3330-0000",
  "3566002020360505",
  "5555 5555 5555 4444",
  "5105-1051-0510-5100",
  "4111111111111111",
  "4012 8888 8888 1881",
  "4222-2222-2222-2",
  "5019717010103742",
  "6331101999990016",
};
const char* const kInvalidNumbers[] = {
  "4111 1111 112", /* too short */
  "41111111111111111115", /* too long */
  "4111-1111-1111-1110", /* wrong Luhn checksum */
  "3056 9309 0259 04aa", /* non-digit characters */
};

}  // namespace

typedef testing::Test CreditCardTest;

// Tests credit card summary string generation.  This test simulates a variety
// of different possible summary strings.  Variations occur based on the
// existence of credit card number, month, and year fields.
TEST_F(CreditCardTest, PreviewSummaryAndObfuscatedNumberStrings) {
  // Case 0: empty credit card.
  CreditCard credit_card0;
  string16 summary0 = credit_card0.Label();
  EXPECT_EQ(string16(), summary0);
  string16 obfuscated0 = credit_card0.ObfuscatedNumber();
  EXPECT_EQ(string16(), obfuscated0);

  // Case 00: Empty credit card with empty strings.
  CreditCard credit_card00;
  autofill_test::SetCreditCardInfo(&credit_card00,
      "John Dillinger", "", "", "");
  string16 summary00 = credit_card00.Label();
  EXPECT_EQ(string16(ASCIIToUTF16("John Dillinger")), summary00);
  string16 obfuscated00 = credit_card00.ObfuscatedNumber();
  EXPECT_EQ(string16(), obfuscated00);

  // Case 1: No credit card number.
  CreditCard credit_card1;
  autofill_test::SetCreditCardInfo(&credit_card1,
      "John Dillinger", "", "01", "2010");
  string16 summary1 = credit_card1.Label();
  EXPECT_EQ(string16(ASCIIToUTF16("John Dillinger")), summary1);
  string16 obfuscated1 = credit_card1.ObfuscatedNumber();
  EXPECT_EQ(string16(), obfuscated1);

  // Case 2: No month.
  CreditCard credit_card2;
  autofill_test::SetCreditCardInfo(&credit_card2,
      "John Dillinger", "5105 1051 0510 5100", "", "2010");
  string16 summary2 = credit_card2.Label();
  EXPECT_EQ(ASCIIToUTF16("************5100"), summary2);
  string16 obfuscated2 = credit_card2.ObfuscatedNumber();
  EXPECT_EQ(ASCIIToUTF16("************5100"), obfuscated2);

  // Case 3: No year.
  CreditCard credit_card3;
  autofill_test::SetCreditCardInfo(&credit_card3,
      "John Dillinger", "5105 1051 0510 5100", "01", "");
  string16 summary3 = credit_card3.Label();
  EXPECT_EQ(ASCIIToUTF16("************5100"), summary3);
  string16 obfuscated3 = credit_card3.ObfuscatedNumber();
  EXPECT_EQ(ASCIIToUTF16("************5100"), obfuscated3);

  // Case 4: Have everything.
  CreditCard credit_card4;
  autofill_test::SetCreditCardInfo(&credit_card4,
      "John Dillinger", "5105 1051 0510 5100", "01", "2010");
  string16 summary4 = credit_card4.Label();
  EXPECT_EQ(ASCIIToUTF16("************5100, Exp: 01/2010"), summary4);
  string16 obfuscated4 = credit_card4.ObfuscatedNumber();
  EXPECT_EQ(ASCIIToUTF16("************5100"), obfuscated4);
}

TEST_F(CreditCardTest, AssignmentOperator) {
  CreditCard a, b;

  // Result of assignment should be logically equal to the original profile.
  autofill_test::SetCreditCardInfo(&a, "John Dillinger",
                                   "123456789012", "01", "2010");
  b = a;
  EXPECT_TRUE(a == b);

  // Assignment to self should not change the profile value.
  a = a;
  EXPECT_TRUE(a == b);
}

TEST_F(CreditCardTest, IsValidCreditCardNumber) {
  for (size_t i = 0; i < arraysize(kValidNumbers); ++i) {
    SCOPED_TRACE(kValidNumbers[i]);
    EXPECT_TRUE(
        CreditCard::IsValidCreditCardNumber(ASCIIToUTF16(kValidNumbers[i])));
  }
  for (size_t i = 0; i < arraysize(kInvalidNumbers); ++i) {
    SCOPED_TRACE(kInvalidNumbers[i]);
    EXPECT_FALSE(
        CreditCard::IsValidCreditCardNumber(ASCIIToUTF16(kInvalidNumbers[i])));
  }
}

TEST_F(CreditCardTest, IsComplete) {
  CreditCard card;
  EXPECT_FALSE(card.IsComplete());
  card.SetInfo(CREDIT_CARD_NAME, ASCIIToUTF16("Wally T. Walrus"));
  EXPECT_FALSE(card.IsComplete());
  card.SetInfo(CREDIT_CARD_EXP_MONTH, ASCIIToUTF16("01"));
  EXPECT_FALSE(card.IsComplete());
  card.SetInfo(CREDIT_CARD_EXP_4_DIGIT_YEAR, ASCIIToUTF16("2014"));

  for (size_t i = 0; i < arraysize(kValidNumbers); ++i) {
    SCOPED_TRACE(kValidNumbers[i]);
    card.SetInfo(CREDIT_CARD_NUMBER, ASCIIToUTF16(kValidNumbers[i]));
    EXPECT_TRUE(card.IsComplete());
  }
  for (size_t i = 0; i < arraysize(kInvalidNumbers); ++i) {
    SCOPED_TRACE(kInvalidNumbers[i]);
    card.SetInfo(CREDIT_CARD_NUMBER, ASCIIToUTF16(kInvalidNumbers[i]));
    EXPECT_FALSE(card.IsComplete());
  }
}

TEST_F(CreditCardTest, InvalidMastercardNumber) {
  CreditCard card;

  autofill_test::SetCreditCardInfo(&card, "Baby Face Nelson",
                                   "5200000000000004", "01", "2010");
  EXPECT_EQ("genericCC", card.type());
}

// Verify that we preserve exactly what the user typed for credit card numbers.
TEST_F(CreditCardTest, SetInfoCreditCardNumber) {
  CreditCard card;

  autofill_test::SetCreditCardInfo(&card, "Bob Dylan",
                                   "4321-5432-6543-xxxx", "07", "2013");
  EXPECT_EQ(ASCIIToUTF16("4321-5432-6543-xxxx"),
            card.GetInfo(CREDIT_CARD_NUMBER));
}

// Verify that we can handle both numeric and named months.
TEST_F(CreditCardTest, SetInfoExpirationMonth) {
  CreditCard card;

  card.SetInfo(CREDIT_CARD_EXP_MONTH, ASCIIToUTF16("05"));
  EXPECT_EQ(ASCIIToUTF16("05"), card.GetInfo(CREDIT_CARD_EXP_MONTH));

  card.SetInfo(CREDIT_CARD_EXP_MONTH, ASCIIToUTF16("7"));
  EXPECT_EQ(ASCIIToUTF16("07"), card.GetInfo(CREDIT_CARD_EXP_MONTH));

  card.SetInfo(CREDIT_CARD_EXP_MONTH, ASCIIToUTF16("January"));
  EXPECT_EQ(ASCIIToUTF16("01"), card.GetInfo(CREDIT_CARD_EXP_MONTH));

  card.SetInfo(CREDIT_CARD_EXP_MONTH, ASCIIToUTF16("Apr"));
  EXPECT_EQ(ASCIIToUTF16("04"), card.GetInfo(CREDIT_CARD_EXP_MONTH));
}
