// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/country_combobox_model.h"

#include <algorithm>
#include <iterator>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "components/autofill/core/browser/autofill_country.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "ui/base/l10n/l10n_util_collator.h"
#include "ui/base/models/combobox_model_observer.h"

// TODO(rouslan): Remove this check. http://crbug.com/337587
#if defined(ENABLE_AUTOFILL_DIALOG)
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_ui.h"
#endif

namespace autofill {

CountryComboboxModel::CountryComboboxModel() {}

CountryComboboxModel::~CountryComboboxModel() {}

void CountryComboboxModel::SetCountries(
    const PersonalDataManager& manager,
    const base::Callback<bool(const std::string&)>& filter) {
  countries_.clear();

  // Insert the default country at the top as well as in the ordered list.
  std::string default_country_code =
      manager.GetDefaultCountryCodeForNewAddress();
  DCHECK(!default_country_code.empty());

  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  if (filter.is_null() || filter.Run(default_country_code)) {
    countries_.push_back(new AutofillCountry(default_country_code, app_locale));
    // The separator item.
    countries_.push_back(NULL);
  }

  // The sorted list of countries.
  std::vector<std::string> available_countries;
  AutofillCountry::GetAvailableCountries(&available_countries);

#if defined(ENABLE_AUTOFILL_DIALOG)
  // Filter out the countries that do not have rules for address input and
  // validation.
  const std::vector<std::string>& addressinput_countries =
      ::i18n::addressinput::GetRegionCodes();
  std::vector<std::string> filtered_countries;
  filtered_countries.reserve(available_countries.size());
  std::set_intersection(available_countries.begin(),
                        available_countries.end(),
                        addressinput_countries.begin(),
                        addressinput_countries.end(),
                        std::back_inserter(filtered_countries));
  available_countries.swap(filtered_countries);
#endif

  std::vector<AutofillCountry*> sorted_countries;
  for (std::vector<std::string>::const_iterator it =
           available_countries.begin(); it != available_countries.end(); ++it) {
    if (filter.is_null() || filter.Run(*it))
      sorted_countries.push_back(new AutofillCountry(*it, app_locale));
  }

  l10n_util::SortStringsUsingMethod(app_locale,
                                    &sorted_countries,
                                    &AutofillCountry::name);
  countries_.insert(countries_.end(),
                    sorted_countries.begin(),
                    sorted_countries.end());
}

int CountryComboboxModel::GetItemCount() const {
  return countries_.size();
}

base::string16 CountryComboboxModel::GetItemAt(int index) {
  AutofillCountry* country = countries_[index];
  if (country)
    return countries_[index]->name();

  // The separator item. Implemented for platforms that don't yet support
  // IsItemSeparatorAt().
  return base::ASCIIToUTF16("---");
}

bool CountryComboboxModel::IsItemSeparatorAt(int index) {
  return !countries_[index];
}

std::string CountryComboboxModel::GetDefaultCountryCode() const {
  return countries_[GetDefaultIndex()]->country_code();
}

}  // namespace autofill
