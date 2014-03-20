// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_dialog_i18n_input.h"

#include "components/autofill/core/browser/field_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libaddressinput/chromium/cpp/include/libaddressinput/address_ui.h"

namespace autofill {
namespace i18ninput {

namespace {

const size_t kNumberOfAddressLinesUS = 7;

}  // namespace

TEST(AutofillDialogI18nInput, USShippingAddress) {
  DetailInputs inputs;
  BuildAddressInputs(common::ADDRESS_TYPE_SHIPPING, "US", &inputs);

  ASSERT_EQ(kNumberOfAddressLinesUS, inputs.size());
  EXPECT_EQ(NAME_FULL, inputs[0].type);
  EXPECT_EQ(ADDRESS_HOME_COUNTRY, inputs[kNumberOfAddressLinesUS - 1].type);
}

TEST(AutofillDialogI18nInput, USBillingAddress) {
  DetailInputs inputs;
  BuildAddressInputs(common::ADDRESS_TYPE_BILLING, "US", &inputs);

  ASSERT_EQ(kNumberOfAddressLinesUS, inputs.size());
  EXPECT_EQ(NAME_BILLING_FULL, inputs[0].type);
  EXPECT_EQ(ADDRESS_BILLING_COUNTRY, inputs[kNumberOfAddressLinesUS - 1].type);
}

TEST(AutofillDialogI18nInput, USCityStateAndZipCodeShareInputRow) {
  DetailInputs inputs;
  BuildAddressInputs(common::ADDRESS_TYPE_SHIPPING, "US", &inputs);
  ASSERT_EQ(kNumberOfAddressLinesUS, inputs.size());

  // Inputs before or after [ City ] [ State ] [ Zip ] should be on other lines.
  EXPECT_NE(inputs[2].length, DetailInput::SHORT);

  const DetailInput& city = inputs[3];
  ASSERT_EQ(ADDRESS_HOME_CITY, city.type);
  EXPECT_EQ(city.length, DetailInput::SHORT);

  const DetailInput& state = inputs[4];
  ASSERT_EQ(ADDRESS_HOME_STATE, state.type);
  EXPECT_EQ(state.length, DetailInput::SHORT);

  const DetailInput& zip = inputs[5];
  ASSERT_EQ(ADDRESS_HOME_ZIP, zip.type);
  EXPECT_EQ(zip.length, DetailInput::SHORT);

  EXPECT_NE(inputs[6].length, DetailInput::SHORT);
}

TEST(AutofillDialogI18nInput, IvoryCoastNoStreetLine2) {
  DetailInputs inputs;
  BuildAddressInputs(common::ADDRESS_TYPE_SHIPPING, "CI", &inputs);
  for (size_t i = 0; i < inputs.size(); ++i) {
    EXPECT_NE(ADDRESS_HOME_LINE2, inputs[i].type);
  }
}

TEST(AutofillDialogI18nInput, FullySupportedCountries) {
  const std::vector<std::string>& regions =
      ::i18n::addressinput::GetRegionCodes();
  for (size_t i = 0; i < regions.size(); ++i) {
    bool should_be_supported = !(regions[i] == "KR" || regions[i] == "CN");
    EXPECT_EQ(should_be_supported, CountryIsFullySupported(regions[i]));
  }
}

}  // namespace i18ninput
}  // namespace autofill
