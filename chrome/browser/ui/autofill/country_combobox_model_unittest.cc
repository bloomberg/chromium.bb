// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/country_combobox_model.h"

#include "components/autofill/core/browser/autofill_country.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libaddressinput/chromium/cpp/include/libaddressinput/address_ui.h"
#include "third_party/libaddressinput/chromium/cpp/include/libaddressinput/address_ui_component.h"

namespace autofill {

TEST(CountryComboboxModel, RespectsManagerDefaultCountry) {
  const std::string test_country = "AQ";
  TestPersonalDataManager manager;
  manager.set_timezone_country_code(test_country);

  CountryComboboxModel model(manager, true);
  EXPECT_EQ(test_country, model.GetDefaultCountryCode());
}

TEST(CountryComboboxModel, AllCountriesHaveComponents) {
  TestPersonalDataManager manager;
  CountryComboboxModel model(manager, true);

  for (int i = 0; i < model.GetItemCount(); ++i) {
    if (model.IsItemSeparatorAt(i))
      continue;

    std::string country_code = model.countries()[i]->country_code();
    std::vector< ::i18n::addressinput::AddressUiComponent> components =
        ::i18n::addressinput::BuildComponents(country_code);
    EXPECT_FALSE(components.empty());
  }
}

TEST(CountryComboboxModel, CountriesWithDependentLocalityNotShown) {
  TestPersonalDataManager manager;
  manager.set_timezone_country_code("KR");
  CountryComboboxModel model(manager, false);
  EXPECT_NE("KR", model.GetDefaultCountryCode());

  for (int i = 0; i < model.GetItemCount(); ++i) {
    ASSERT_FALSE(model.IsItemSeparatorAt(i));
    std::string country_code = model.countries()[i]->country_code();
    std::vector< ::i18n::addressinput::AddressUiComponent> components =
        ::i18n::addressinput::BuildComponents(country_code);
    ASSERT_FALSE(components.empty());
    for (size_t j = 0; j < components.size(); ++j) {
      EXPECT_NE(components[j].field, ::i18n::addressinput::DEPENDENT_LOCALITY);
    }
  }
}

}  // namespace autofill
