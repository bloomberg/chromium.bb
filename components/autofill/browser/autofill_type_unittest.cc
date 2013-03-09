// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/browser/autofill_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(AutofillTypeTest, AutofillTypes) {
  // No server data.
  AutofillType none(NO_SERVER_DATA);
  EXPECT_EQ(NO_SERVER_DATA, none.field_type());
  EXPECT_EQ(AutofillType::NO_GROUP, none.group());

  // Unknown type.
  AutofillType unknown(UNKNOWN_TYPE);
  EXPECT_EQ(UNKNOWN_TYPE, unknown.field_type());
  EXPECT_EQ(AutofillType::NO_GROUP, unknown.group());

  // Type with group but no subgroup.
  AutofillType first(NAME_FIRST);
  EXPECT_EQ(NAME_FIRST, first.field_type());
  EXPECT_EQ(AutofillType::NAME, first.group());

  // Type with group and subgroup.
  AutofillType phone(PHONE_HOME_NUMBER);
  EXPECT_EQ(PHONE_HOME_NUMBER, phone.field_type());
  EXPECT_EQ(AutofillType::PHONE, phone.group());

  // Last value, to check any offset errors.
  AutofillType last(COMPANY_NAME);
  EXPECT_EQ(COMPANY_NAME, last.field_type());
  EXPECT_EQ(AutofillType::COMPANY, last.group());

  // Boundary (error) condition.
  AutofillType boundary(MAX_VALID_FIELD_TYPE);
  EXPECT_EQ(UNKNOWN_TYPE, boundary.field_type());
  EXPECT_EQ(AutofillType::NO_GROUP, boundary.group());

  // Beyond the boundary (error) condition.
  AutofillType beyond(static_cast<AutofillFieldType>(MAX_VALID_FIELD_TYPE+10));
  EXPECT_EQ(UNKNOWN_TYPE, beyond.field_type());
  EXPECT_EQ(AutofillType::NO_GROUP, beyond.group());

  // In-between value.  Missing from enum but within range.  Error condition.
  AutofillType between(static_cast<AutofillFieldType>(16));
  EXPECT_EQ(UNKNOWN_TYPE, between.field_type());
  EXPECT_EQ(AutofillType::NO_GROUP, between.group());
}

}  // namespace
