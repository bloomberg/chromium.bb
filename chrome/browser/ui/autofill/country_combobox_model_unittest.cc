// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/country_combobox_model.h"

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/browser_process.h"
#include "chrome/test/base/testing_profile.h"
#include "components/autofill/core/browser/autofill_country.h"
#include "components/autofill/core/browser/test_personal_data_manager.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/libaddressinput/chromium/cpp/include/libaddressinput/address_ui.h"
#include "third_party/libaddressinput/chromium/cpp/include/libaddressinput/address_ui_component.h"

namespace autofill {

class CountryComboboxModelTest : public testing::Test {
 public:
  CountryComboboxModelTest() {
    manager_.Init(NULL, profile_.GetPrefs(), false);
    manager_.set_timezone_country_code("KR");
    CreateModel(true);
  }
  virtual ~CountryComboboxModelTest() {}

 protected:
  void CreateModel(bool show_partially_supported_countries) {
    model_.reset(new CountryComboboxModel(
        manager_, show_partially_supported_countries));
  }

  TestPersonalDataManager* manager() { return &manager_; }
  CountryComboboxModel* model() { return model_.get(); }

 private:
  // NB: order is important here - |profile_| must go down after |manager_|.
  TestingProfile profile_;
  TestPersonalDataManager manager_;
  scoped_ptr<CountryComboboxModel> model_;
};

TEST_F(CountryComboboxModelTest, DefaultCountryCode) {
  std::string default_country = model()->GetDefaultCountryCode();
  EXPECT_EQ(manager()->GetDefaultCountryCodeForNewAddress(), default_country);

  AutofillCountry country(default_country,
                          g_browser_process->GetApplicationLocale());
  EXPECT_EQ(country.name(), model()->GetItemAt(0));
}

TEST_F(CountryComboboxModelTest, AllCountriesHaveComponents) {
  for (int i = 0; i < model()->GetItemCount(); ++i) {
    if (model()->IsItemSeparatorAt(i))
      continue;

    std::string country_code = model()->countries()[i]->country_code();
    std::vector< ::i18n::addressinput::AddressUiComponent> components =
        ::i18n::addressinput::BuildComponents(country_code);
    EXPECT_FALSE(components.empty());
  }
}

TEST_F(CountryComboboxModelTest, CountriesWithDependentLocalityNotShown) {
  CreateModel(false);
  EXPECT_NE("KR", model()->GetDefaultCountryCode());

  for (int i = 0; i < model()->GetItemCount(); ++i) {
    ASSERT_FALSE(model()->IsItemSeparatorAt(i));
    std::string country_code = model()->countries()[i]->country_code();
    std::vector< ::i18n::addressinput::AddressUiComponent> components =
        ::i18n::addressinput::BuildComponents(country_code);
    ASSERT_FALSE(components.empty());
    for (size_t j = 0; j < components.size(); ++j) {
      EXPECT_NE(components[j].field, ::i18n::addressinput::DEPENDENT_LOCALITY);
    }
  }
}

}  // namespace autofill
