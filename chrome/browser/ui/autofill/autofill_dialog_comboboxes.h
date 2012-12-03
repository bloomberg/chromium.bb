// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_COMBOBOXES_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_COMBOBOXES_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/string16.h"
#include "ui/base/models/combobox_model.h"

namespace autofill {

// A model for the comboboxes that allow the user to select from different sets
// of known data.
class SuggestionsComboboxModel : public ui::ComboboxModel {
 public:
  SuggestionsComboboxModel();
  virtual ~SuggestionsComboboxModel();

  // Adds an item and its identifying key to the model.
  void AddItem(const std::string& key, const string16& display_label);

  // Returns the ID key for the item at |index|.
  std::string GetItemKeyAt(int index) const;

  // ui::Combobox implementation:
  virtual int GetItemCount() const OVERRIDE;
  virtual string16 GetItemAt(int index) OVERRIDE;

 private:
  // The items this model represents, in presentation order. The first
  // string is the "key" which identifies the item. The second is the
  // display string for the item.
  std::vector<std::pair<std::string, string16> > items_;

  DISALLOW_COPY_AND_ASSIGN(SuggestionsComboboxModel);
};

// A model for possible months in the Gregorian calendar.
class MonthComboboxModel : public ui::ComboboxModel {
 public:
  MonthComboboxModel();
  virtual ~MonthComboboxModel();

  // ui::Combobox implementation:
  virtual int GetItemCount() const OVERRIDE;
  virtual string16 GetItemAt(int index) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(MonthComboboxModel);
};

// A model for years between now and a decade hence.
class YearComboboxModel : public ui::ComboboxModel {
 public:
  YearComboboxModel();
  virtual ~YearComboboxModel();

  // ui::Combobox implementation:
  virtual int GetItemCount() const OVERRIDE;
  virtual string16 GetItemAt(int index) OVERRIDE;

 private:
  // The current year (e.g., 2012).
  int this_year_;

  DISALLOW_COPY_AND_ASSIGN(YearComboboxModel);
};

}  // autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_COMBOBOXES_H_
