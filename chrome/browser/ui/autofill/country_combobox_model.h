// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_COUNTRY_COMBOBOX_MODEL_H_
#define CHROME_BROWSER_UI_AUTOFILL_COUNTRY_COMBOBOX_MODEL_H_

#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_vector.h"
#include "base/observer_list.h"
#include "base/strings/string16.h"
#include "ui/base/models/combobox_model.h"

namespace autofill {

class AutofillCountry;
class PersonalDataManager;

// A model for countries to be used to enter addresses.
class CountryComboboxModel : public ui::ComboboxModel {
 public:
  explicit CountryComboboxModel(const PersonalDataManager& manager);
  virtual ~CountryComboboxModel();

  // ui::ComboboxModel implementation:
  virtual int GetItemCount() const OVERRIDE;
  virtual base::string16 GetItemAt(int index) OVERRIDE;
  virtual bool IsItemSeparatorAt(int index) OVERRIDE;
  virtual int GetDefaultIndex() const OVERRIDE;
  virtual void AddObserver(ui::ComboboxModelObserver* observer) OVERRIDE;
  virtual void RemoveObserver(ui::ComboboxModelObserver* observer) OVERRIDE;

  const std::vector<AutofillCountry*>& countries() const {
    return countries_.get();
  }

  // Clears the default country.
  void ResetDefault();

  // Changes |country_code| to the default country.
  void SetDefaultCountry(const std::string& country_code);

  // Returns the default country code for this model.
  std::string GetDefaultCountryCode() const;

 private:
  // Changes |default_index_| to |index| after checking it's sane. Notifies
  // observers if |default_index_| changed.
  void SetDefaultIndex(int index);

  // The countries to show in the model, including NULL for entries that are
  // not countries (the separator entry).
  ScopedVector<AutofillCountry> countries_;

  // The index of the default country.
  int default_index_;

  ObserverList<ui::ComboboxModelObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(CountryComboboxModel);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_COUNTRY_COMBOBOX_MODEL_H_
