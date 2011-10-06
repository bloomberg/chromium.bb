// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

TEST(AutofillTypeTest, Basic) {
  // No server data.
  AutofillType none(NO_SERVER_DATA);
  EXPECT_EQ(NO_SERVER_DATA, none.field_type());
  EXPECT_EQ(AutofillType::NO_GROUP, none.group());
  EXPECT_EQ(AutofillType::NO_SUBGROUP, none.subgroup());

  // Unknown type.
  AutofillType unknown(UNKNOWN_TYPE);
  EXPECT_EQ(UNKNOWN_TYPE, unknown.field_type());
  EXPECT_EQ(AutofillType::NO_GROUP, unknown.group());
  EXPECT_EQ(AutofillType::NO_SUBGROUP, unknown.subgroup());

  // Type with group but no subgroup.
  AutofillType first(NAME_FIRST);
  EXPECT_EQ(NAME_FIRST, first.field_type());
  EXPECT_EQ(AutofillType::NAME, first.group());
  EXPECT_EQ(AutofillType::NO_SUBGROUP, first.subgroup());

  // Type with group and subgroup.
  AutofillType phone(PHONE_HOME_NUMBER);
  EXPECT_EQ(PHONE_HOME_NUMBER, phone.field_type());
  EXPECT_EQ(AutofillType::PHONE_HOME, phone.group());
  EXPECT_EQ(AutofillType::PHONE_NUMBER, phone.subgroup());

  // Last value, to check any offset errors.
  AutofillType last(COMPANY_NAME);
  EXPECT_EQ(COMPANY_NAME, last.field_type());
  EXPECT_EQ(AutofillType::COMPANY, last.group());
  EXPECT_EQ(AutofillType::NO_SUBGROUP, last.subgroup());

  // Boundary (error) condition.
  AutofillType boundary(MAX_VALID_FIELD_TYPE);
  EXPECT_EQ(UNKNOWN_TYPE, boundary.field_type());
  EXPECT_EQ(AutofillType::NO_GROUP, boundary.group());
  EXPECT_EQ(AutofillType::NO_SUBGROUP, boundary.subgroup());

  // Beyond the boundary (error) condition.
  AutofillType beyond(static_cast<AutofillFieldType>(MAX_VALID_FIELD_TYPE+10));
  EXPECT_EQ(UNKNOWN_TYPE, beyond.field_type());
  EXPECT_EQ(AutofillType::NO_GROUP, beyond.group());
  EXPECT_EQ(AutofillType::NO_SUBGROUP, beyond.subgroup());

  // In-between value.  Missing from enum but within range.  Error condition.
  AutofillType between(static_cast<AutofillFieldType>(16));
  EXPECT_EQ(UNKNOWN_TYPE, between.field_type());
  EXPECT_EQ(AutofillType::NO_GROUP, between.group());
  EXPECT_EQ(AutofillType::NO_SUBGROUP, between.subgroup());
}

}  // namespace
