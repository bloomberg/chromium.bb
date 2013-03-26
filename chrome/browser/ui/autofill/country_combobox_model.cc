// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/country_combobox_model.h"

#include "components/autofill/browser/autofill_country.h"
#include "ui/base/l10n/l10n_util_collator.h"

namespace autofill {

CountryComboboxModel::CountryComboboxModel() {
  // Insert the default country at the top as well as in the ordered list.
  std::string app_locale = AutofillCountry::ApplicationLocale();
  std::string default_country_code =
      AutofillCountry::CountryCodeForLocale(app_locale);
  countries_.push_back(new AutofillCountry(default_country_code, app_locale));
  // The separator item.
  countries_.push_back(NULL);

  // The sorted list of countries.
  std::vector<std::string> country_codes;
  AutofillCountry::GetAvailableCountries(&country_codes);
  std::vector<AutofillCountry*> sorted_countries;

  for (std::vector<std::string>::iterator iter = country_codes.begin();
       iter != country_codes.end(); ++iter) {
    sorted_countries.push_back(new AutofillCountry(*iter, app_locale));
  }

  l10n_util::SortStringsUsingMethod(app_locale,
                                    &sorted_countries,
                                    &AutofillCountry::name);
  countries_.insert(countries_.end(),
                    sorted_countries.begin(),
                    sorted_countries.end());
}

CountryComboboxModel::~CountryComboboxModel() {}

int CountryComboboxModel::GetItemCount() const {
  return countries_.size();
}

string16 CountryComboboxModel::GetItemAt(int index) {
  AutofillCountry* country = countries_[index];
  if (country)
    return countries_[index]->name();

  // The separator item. Implemented for platforms that don't yet support
  // IsItemSeparatorAt().
  return ASCIIToUTF16("---");
}

bool CountryComboboxModel::IsItemSeparatorAt(int index) {
  return !countries_[index];
}

}  // namespace autofill
