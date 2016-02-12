// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/scoped_vector.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/form_field.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::ASCIIToUTF16;

namespace autofill {

TEST(FormFieldTest, Match) {
  AutofillField field;

  // Empty strings match.
  EXPECT_TRUE(FormField::Match(&field, base::string16(),
                               FormField::MATCH_LABEL));

  // Empty pattern matches non-empty string.
  field.label = ASCIIToUTF16("a");
  EXPECT_TRUE(FormField::Match(&field, base::string16(),
                               FormField::MATCH_LABEL));

  // Strictly empty pattern matches empty string.
  field.label = base::string16();
  EXPECT_TRUE(FormField::Match(&field, ASCIIToUTF16("^$"),
              FormField::MATCH_LABEL));

  // Strictly empty pattern does not match non-empty string.
  field.label = ASCIIToUTF16("a");
  EXPECT_FALSE(FormField::Match(&field, ASCIIToUTF16("^$"),
               FormField::MATCH_LABEL));

  // Non-empty pattern doesn't match empty string.
  field.label = base::string16();
  EXPECT_FALSE(FormField::Match(&field, ASCIIToUTF16("a"),
               FormField::MATCH_LABEL));

  // Beginning of line.
  field.label = ASCIIToUTF16("head_tail");
  EXPECT_TRUE(FormField::Match(&field, ASCIIToUTF16("^head"),
              FormField::MATCH_LABEL));
  EXPECT_FALSE(FormField::Match(&field, ASCIIToUTF16("^tail"),
               FormField::MATCH_LABEL));

  // End of line.
  field.label = ASCIIToUTF16("head_tail");
  EXPECT_FALSE(FormField::Match(&field, ASCIIToUTF16("head$"),
               FormField::MATCH_LABEL));
  EXPECT_TRUE(FormField::Match(&field, ASCIIToUTF16("tail$"),
              FormField::MATCH_LABEL));

  // Exact.
  field.label = ASCIIToUTF16("head_tail");
  EXPECT_FALSE(FormField::Match(&field, ASCIIToUTF16("^head$"),
               FormField::MATCH_LABEL));
  EXPECT_FALSE(FormField::Match(&field, ASCIIToUTF16("^tail$"),
               FormField::MATCH_LABEL));
  EXPECT_TRUE(FormField::Match(&field, ASCIIToUTF16("^head_tail$"),
              FormField::MATCH_LABEL));

  // Escaped dots.
  field.label = ASCIIToUTF16("m.i.");
  // Note: This pattern is misleading as the "." characters are wild cards.
  EXPECT_TRUE(FormField::Match(&field, ASCIIToUTF16("m.i."),
              FormField::MATCH_LABEL));
  EXPECT_TRUE(FormField::Match(&field, ASCIIToUTF16("m\\.i\\."),
              FormField::MATCH_LABEL));
  field.label = ASCIIToUTF16("mXiX");
  EXPECT_TRUE(FormField::Match(&field, ASCIIToUTF16("m.i."),
              FormField::MATCH_LABEL));
  EXPECT_FALSE(FormField::Match(&field, ASCIIToUTF16("m\\.i\\."),
               FormField::MATCH_LABEL));

  // Repetition.
  field.label = ASCIIToUTF16("headtail");
  EXPECT_TRUE(FormField::Match(&field, ASCIIToUTF16("head.*tail"),
              FormField::MATCH_LABEL));
  field.label = ASCIIToUTF16("headXtail");
  EXPECT_TRUE(FormField::Match(&field, ASCIIToUTF16("head.*tail"),
              FormField::MATCH_LABEL));
  field.label = ASCIIToUTF16("headXXXtail");
  EXPECT_TRUE(FormField::Match(&field, ASCIIToUTF16("head.*tail"),
              FormField::MATCH_LABEL));
  field.label = ASCIIToUTF16("headtail");
  EXPECT_FALSE(FormField::Match(&field, ASCIIToUTF16("head.+tail"),
               FormField::MATCH_LABEL));
  field.label = ASCIIToUTF16("headXtail");
  EXPECT_TRUE(FormField::Match(&field, ASCIIToUTF16("head.+tail"),
              FormField::MATCH_LABEL));
  field.label = ASCIIToUTF16("headXXXtail");
  EXPECT_TRUE(FormField::Match(&field, ASCIIToUTF16("head.+tail"),
              FormField::MATCH_LABEL));

  // Alternation.
  field.label = ASCIIToUTF16("head_tail");
  EXPECT_TRUE(FormField::Match(&field, ASCIIToUTF16("head|other"),
              FormField::MATCH_LABEL));
  EXPECT_TRUE(FormField::Match(&field, ASCIIToUTF16("tail|other"),
              FormField::MATCH_LABEL));
  EXPECT_FALSE(FormField::Match(&field, ASCIIToUTF16("bad|good"),
               FormField::MATCH_LABEL));

  // Case sensitivity.
  field.label = ASCIIToUTF16("xxxHeAd_tAiLxxx");
  EXPECT_TRUE(FormField::Match(&field, ASCIIToUTF16("head_tail"),
              FormField::MATCH_LABEL));

  // Word boundaries.
  field.label = ASCIIToUTF16("contains word:");
  EXPECT_TRUE(FormField::Match(&field, ASCIIToUTF16("\\bword\\b"),
                               FormField::MATCH_LABEL));
  EXPECT_FALSE(FormField::Match(&field, ASCIIToUTF16("\\bcon\\b"),
                                FormField::MATCH_LABEL));
  // Make sure the circumflex in 'crepe' is not treated as a word boundary.
  field.label = base::UTF8ToUTF16("cr" "\xC3\xAA" "pe");
  EXPECT_FALSE(FormField::Match(&field, ASCIIToUTF16("\\bcr\\b"),
                                FormField::MATCH_LABEL));
}

// Test that we ignore checkable elements.
TEST(FormFieldTest, ParseFormFields) {
  ScopedVector<AutofillField> fields;
  FormFieldData field_data;
  field_data.form_control_type = "text";

  field_data.label = ASCIIToUTF16("Address line1");
  fields.push_back(new AutofillField(field_data, field_data.label));

  field_data.is_checkable = true;
  field_data.label = ASCIIToUTF16("Is PO Box");
  fields.push_back(new AutofillField(field_data, field_data.label));

  // reset |is_checkable| to false.
  field_data.is_checkable = false;

  field_data.label = ASCIIToUTF16("Address line2");
  fields.push_back(new AutofillField(field_data, field_data.label));

  // Does not parse since there are only 2 recognized fields.
  ASSERT_TRUE(FormField::ParseFormFields(fields.get(), true).empty());

  field_data.label = ASCIIToUTF16("City");
  fields.push_back(new AutofillField(field_data, field_data.label));

  // Checkable element shouldn't interfere with inference of Address line2.
  const ServerFieldTypeMap field_type_map =
      FormField::ParseFormFields(fields.get(), true);
  ASSERT_EQ(3U, field_type_map.size());

  EXPECT_EQ(ADDRESS_HOME_LINE1,
            field_type_map.find(ASCIIToUTF16("Address line1"))->second);
  EXPECT_EQ(ADDRESS_HOME_LINE2,
            field_type_map.find(ASCIIToUTF16("Address line2"))->second);
}

}  // namespace autofill
