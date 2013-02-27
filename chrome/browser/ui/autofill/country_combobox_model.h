// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_COUNTRY_COMBOBOX_MODEL_H_
#define CHROME_BROWSER_UI_AUTOFILL_COUNTRY_COMBOBOX_MODEL_H_

#include <vector>

#include "base/compiler_specific.h"
#include "base/memory/scoped_vector.h"
#include "base/string16.h"
#include "ui/base/models/combobox_model.h"

class AutofillCountry;

namespace autofill {

// A model for countries to be used to enter addresses.
class CountryComboboxModel : public ui::ComboboxModel {
 public:
  CountryComboboxModel();
  virtual ~CountryComboboxModel();

  // ui::Combobox implementation:
  virtual int GetItemCount() const OVERRIDE;
  virtual string16 GetItemAt(int index) OVERRIDE;

  const std::vector<AutofillCountry*>& countries() const {
    return countries_.get();
  }

 private:
  // The countries to show in the model, including NULL for entries that are
  // not countries (the separator entry).
  ScopedVector<AutofillCountry> countries_;

  DISALLOW_COPY_AND_ASSIGN(CountryComboboxModel);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_COUNTRY_COMBOBOX_MODEL_H_
