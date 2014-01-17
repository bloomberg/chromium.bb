// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/country_combobox_model.h"

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "components/autofill/core/browser/autofill_country.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "ui/base/l10n/l10n_util_collator.h"
#include "ui/base/models/combobox_model_observer.h"

namespace autofill {

CountryComboboxModel::CountryComboboxModel(const PersonalDataManager& manager)
    : default_index_(0) {
  // Insert the default country at the top as well as in the ordered list.
  const std::string& app_locale = g_browser_process->GetApplicationLocale();
  std::string default_country_code =
      manager.GetDefaultCountryCodeForNewAddress();
  DCHECK(!default_country_code.empty());

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

int CountryComboboxModel::GetDefaultIndex() const {
  return default_index_;
}

void CountryComboboxModel::AddObserver(ui::ComboboxModelObserver* observer) {
  observers_.AddObserver(observer);
}

void CountryComboboxModel::RemoveObserver(ui::ComboboxModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

void CountryComboboxModel::ResetDefault() {
  SetDefaultIndex(0);
}

void CountryComboboxModel::SetDefaultCountry(const std::string& country_code) {
  DCHECK_EQ(2U, country_code.length());

  for (size_t i = 0; i < countries_.size(); ++i) {
    if (countries_[i] && countries_[i]->country_code() == country_code) {
      SetDefaultIndex(i);
      return;
    }
  }

  NOTREACHED();
}

std::string CountryComboboxModel::GetDefaultCountryCode() const {
  return countries_[default_index_]->country_code();
}

void CountryComboboxModel::SetDefaultIndex(int index) {
  DCHECK_GE(index, 0);
  DCHECK_LT(index, static_cast<int>(countries_.size()));
  DCHECK(!IsItemSeparatorAt(index));

  if (index == default_index_)
    return;

  default_index_ = index;
  FOR_EACH_OBSERVER(ui::ComboboxModelObserver, observers_,
                    OnComboboxModelChanged(this));
}

}  // namespace autofill
