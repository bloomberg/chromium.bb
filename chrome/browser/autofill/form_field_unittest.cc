// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/form_field.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(FormFieldTest, Match) {
  AutofillField field;

  // Empty strings match.
  EXPECT_TRUE(FormField::Match(&field, string16(), true));

  // Empty pattern matches non-empty string.
  field.label = ASCIIToUTF16("a");
  EXPECT_TRUE(FormField::Match(&field, string16(), true));

  // Strictly empty pattern matches empty string.
  field.label = ASCIIToUTF16("");
  EXPECT_TRUE(FormField::Match(&field, ASCIIToUTF16("^$"), true));

  // Strictly empty pattern does not match non-empty string.
  field.label = ASCIIToUTF16("a");
  EXPECT_FALSE(FormField::Match(&field, ASCIIToUTF16("^$"), true));

  // Non-empty pattern doesn't match empty string.
  field.label = string16();
  EXPECT_FALSE(FormField::Match(&field, ASCIIToUTF16("a"), true));

  // Beginning of line.
  field.label = ASCIIToUTF16("head_tail");
  EXPECT_TRUE(FormField::Match(&field, ASCIIToUTF16("^head"), true));
  EXPECT_FALSE(FormField::Match(&field, ASCIIToUTF16("^tail"), true));

  // End of line.
  field.label = ASCIIToUTF16("head_tail");
  EXPECT_FALSE(FormField::Match(&field, ASCIIToUTF16("head$"), true));
  EXPECT_TRUE(FormField::Match(&field, ASCIIToUTF16("tail$"), true));

  // Exact.
  field.label = ASCIIToUTF16("head_tail");
  EXPECT_FALSE(FormField::Match(&field, ASCIIToUTF16("^head$"), true));
  EXPECT_FALSE(FormField::Match(&field, ASCIIToUTF16("^tail$"), true));
  EXPECT_TRUE(FormField::Match(&field, ASCIIToUTF16("^head_tail$"), true));

  // Escaped dots.
  field.label = ASCIIToUTF16("m.i.");
  // Note: This pattern is misleading as the "." characters are wild cards.
  EXPECT_TRUE(FormField::Match(&field, ASCIIToUTF16("m.i."), true));
  EXPECT_TRUE(FormField::Match(&field, ASCIIToUTF16("m\\.i\\."), true));
  field.label = ASCIIToUTF16("mXiX");
  EXPECT_TRUE(FormField::Match(&field, ASCIIToUTF16("m.i."), true));
  EXPECT_FALSE(FormField::Match(&field, ASCIIToUTF16("m\\.i\\."), true));

  // Repetition.
  field.label = ASCIIToUTF16("headtail");
  EXPECT_TRUE(FormField::Match(&field, ASCIIToUTF16("head.*tail"), true));
  field.label = ASCIIToUTF16("headXtail");
  EXPECT_TRUE(FormField::Match(&field, ASCIIToUTF16("head.*tail"), true));
  field.label = ASCIIToUTF16("headXXXtail");
  EXPECT_TRUE(FormField::Match(&field, ASCIIToUTF16("head.*tail"), true));
  field.label = ASCIIToUTF16("headtail");
  EXPECT_FALSE(FormField::Match(&field, ASCIIToUTF16("head.+tail"), true));
  field.label = ASCIIToUTF16("headXtail");
  EXPECT_TRUE(FormField::Match(&field, ASCIIToUTF16("head.+tail"), true));
  field.label = ASCIIToUTF16("headXXXtail");
  EXPECT_TRUE(FormField::Match(&field, ASCIIToUTF16("head.+tail"), true));

  // Alternation.
  field.label = ASCIIToUTF16("head_tail");
  EXPECT_TRUE(FormField::Match(&field, ASCIIToUTF16("head|other"), true));
  EXPECT_TRUE(FormField::Match(&field, ASCIIToUTF16("tail|other"), true));
  EXPECT_FALSE(FormField::Match(&field, ASCIIToUTF16("bad|good"), true));

  // Case sensitivity.
  field.label = ASCIIToUTF16("xxxHeAd_tAiLxxx");
  EXPECT_TRUE(FormField::Match(&field, ASCIIToUTF16("head_tail"), true));
}

}  // namespace
