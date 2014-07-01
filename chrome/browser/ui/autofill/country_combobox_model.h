// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_COUNTRY_COMBOBOX_MODEL_H_
#define CHROME_BROWSER_UI_AUTOFILL_COUNTRY_COMBOBOX_MODEL_H_

#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_vector.h"
#include "ui/base/models/combobox_model.h"

namespace autofill {

class AutofillCountry;
class PersonalDataManager;

// A model for countries to be used to enter addresses.
class CountryComboboxModel : public ui::ComboboxModel {
 public:
  CountryComboboxModel();
  virtual ~CountryComboboxModel();

  // |filter| is passed each known country's country code. If |filter| returns
  // true, an item for that country is added to the model (else it's omitted).
  // |manager| determines the default choice.
  void SetCountries(const PersonalDataManager& manager,
                    const base::Callback<bool(const std::string&)>& filter);

  // ui::ComboboxModel implementation:
  virtual int GetItemCount() const OVERRIDE;
  virtual base::string16 GetItemAt(int index) OVERRIDE;
  virtual bool IsItemSeparatorAt(int index) OVERRIDE;

  const std::vector<AutofillCountry*>& countries() const {
    return countries_.get();
  }

  // Returns the default country code for this model.
  std::string GetDefaultCountryCode() const;

 private:
  // The countries to show in the model, including NULL for entries that are
  // not countries (the separator entry).
  ScopedVector<AutofillCountry> countries_;

  DISALLOW_COPY_AND_ASSIGN(CountryComboboxModel);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_COUNTRY_COMBOBOX_MODEL_H_
