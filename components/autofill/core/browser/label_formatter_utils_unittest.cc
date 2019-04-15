// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/label_formatter_utils.h"

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/grit/components_scaled_resources.h"
#include "components/strings/grit/components_strings.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {
using label_formatter_groups::kAddress;
using label_formatter_groups::kEmail;
using label_formatter_groups::kName;
using label_formatter_groups::kPhone;

}  // namespace

TEST(LabelFormatterUtilsTest, DetermineGroupsForHomeNameAndAddress) {
  const std::vector<ServerFieldType> field_types{
      NAME_FIRST,        NAME_LAST,          ADDRESS_HOME_LINE1,
      ADDRESS_HOME_CITY, ADDRESS_HOME_STATE, ADDRESS_HOME_ZIP};

  const uint32_t expected_group_bitmask = kName | kAddress;
  const uint32_t group_bitmask = DetermineGroups(field_types);
  EXPECT_EQ(expected_group_bitmask, group_bitmask);
}

TEST(LabelFormatterUtilsTest, DetermineGroupsForBillingNameAndAddress) {
  const std::vector<ServerFieldType> field_types{
      NAME_BILLING_FULL, ADDRESS_BILLING_LINE1, ADDRESS_BILLING_CITY,
      ADDRESS_BILLING_STATE, ADDRESS_BILLING_ZIP};

  const uint32_t expected_group_bitmask = kName | kAddress;
  const uint32_t group_bitmask = DetermineGroups(field_types);
  EXPECT_EQ(expected_group_bitmask, group_bitmask);
}

TEST(LabelFormatterUtilsTest, DetermineGroupsForHomeNamePhoneAndEmail) {
  const std::vector<ServerFieldType> field_types{
      NAME_FULL, PHONE_HOME_CITY_AND_NUMBER, EMAIL_ADDRESS};

  const uint32_t expected_group_bitmask = kName | kPhone | kEmail;
  const uint32_t group_bitmask = DetermineGroups(field_types);
  EXPECT_EQ(expected_group_bitmask, group_bitmask);
}

TEST(LabelFormatterUtilsTest, DetermineGroupsForBillingNamePhoneAndEmail) {
  const std::vector<ServerFieldType> field_types{
      NAME_BILLING_FULL, PHONE_BILLING_WHOLE_NUMBER, EMAIL_ADDRESS};

  const uint32_t expected_group_bitmask = kName | kPhone | kEmail;
  const uint32_t group_bitmask = DetermineGroups(field_types);
  EXPECT_EQ(expected_group_bitmask, group_bitmask);
}

TEST(LabelFormatterUtilsTest, DetermineGroupsForUnknownServerFieldType) {
  const std::vector<ServerFieldType> field_types{UNKNOWN_TYPE, NAME_FULL,
                                                 ADDRESS_HOME_ZIP};

  const uint32_t expected_group_bitmask = kName | kAddress;
  const uint32_t group_bitmask = DetermineGroups(field_types);
  EXPECT_EQ(expected_group_bitmask, group_bitmask);
}

TEST(LabelFormatterUtilsTest, DetermineGroupsForNoServerFieldTypes) {
  const std::vector<ServerFieldType> field_types =
      std::vector<ServerFieldType>();
  const uint32_t expected_group_bitmask = 0;
  const uint32_t group_bitmask = DetermineGroups(field_types);
  EXPECT_EQ(expected_group_bitmask, group_bitmask);
}

TEST(LabelFormatterUtilsTest, ConstructLabelLine) {
  EXPECT_EQ(base::string16(), ConstructLabelLine({}));

  base::string16 name = base::ASCIIToUTF16("Blaise Pascal");
  base::string16 phone = base::ASCIIToUTF16("01 53 01 82 00");
  base::string16 email = base::ASCIIToUTF16("b.pascal@orange.fr");

  base::string16 separator =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_SUGGESTION_LABEL_SEPARATOR);

  EXPECT_EQ(name, ConstructLabelLine({name}));
  EXPECT_EQ(base::JoinString({name, separator, phone, separator, email},
                             base::string16()),
            ConstructLabelLine({name, phone, email}));
}

}  // namespace autofill