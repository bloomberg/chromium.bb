// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_field.h"
#include "chrome/browser/autofill/field_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(AutofillFieldTest, Type) {
  AutofillField field;
  ASSERT_EQ(NO_SERVER_DATA, field.server_type());
  ASSERT_EQ(UNKNOWN_TYPE, field.heuristic_type());

  // |server_type_| is NO_SERVER_DATA, so |heuristic_type_| is returned.
  EXPECT_EQ(UNKNOWN_TYPE, field.type());

  // Set the heuristic type and check it.
  field.set_heuristic_type(NAME_FIRST);
  EXPECT_EQ(NAME_FIRST, field.type());

  // Set the server type and check it.
  field.set_server_type(ADDRESS_BILLING_LINE1);
  EXPECT_EQ(ADDRESS_BILLING_LINE1, field.type());

  // Remove the server type to make sure the heuristic type is preserved.
  field.set_server_type(NO_SERVER_DATA);
  EXPECT_EQ(NAME_FIRST, field.type());
}

TEST(AutofillFieldTest, IsEmpty) {
  AutofillField field;
  ASSERT_EQ(string16(), field.value());

  // Field value is empty.
  EXPECT_TRUE(field.IsEmpty());

  // Field value is non-empty.
  field.set_value(ASCIIToUTF16("Value"));
  EXPECT_FALSE(field.IsEmpty());
}

TEST(AutofillFieldTest, FieldSignature) {
  AutofillField field;
  ASSERT_EQ(string16(), field.name());
  ASSERT_EQ(string16(), field.form_control_type());

  // Signature is empty.
  EXPECT_EQ("2085434232", field.FieldSignature());

  // Field name is set.
  field.set_name(ASCIIToUTF16("Name"));
  EXPECT_EQ("1606968241", field.FieldSignature());

  // Field form control type is set.
  field.set_form_control_type(ASCIIToUTF16("Text"));
  EXPECT_EQ("4246049809", field.FieldSignature());

  // Heuristic type does not affect FieldSignature.
  field.set_heuristic_type(NAME_FIRST);
  EXPECT_EQ("4246049809", field.FieldSignature());

  // Server type does not affect FieldSignature.
  field.set_server_type(NAME_LAST);
  EXPECT_EQ("4246049809", field.FieldSignature());
}

TEST(AutofillFieldTest, IsFieldFillable) {
  AutofillField field;
  ASSERT_EQ(UNKNOWN_TYPE, field.type());

  // Type is unknown.
  EXPECT_FALSE(field.IsFieldFillable());

  // Only heuristic type is set.
  field.set_heuristic_type(NAME_FIRST);
  EXPECT_TRUE(field.IsFieldFillable());

  // Only server type is set.
  field.set_heuristic_type(UNKNOWN_TYPE);
  field.set_server_type(NAME_LAST);
  EXPECT_TRUE(field.IsFieldFillable());

  // Both types set.
  field.set_heuristic_type(NAME_FIRST);
  field.set_server_type(NAME_LAST);
  EXPECT_TRUE(field.IsFieldFillable());
}

}  // namespace
