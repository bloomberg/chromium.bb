// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_dialog_comboboxes.h"

#include "base/string_number_conversions.h"
#include "base/stringprintf.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"

namespace autofill {

// SuggestionsComboboxModel ----------------------------------------------------

SuggestionsComboboxModel::SuggestionsComboboxModel() {}

SuggestionsComboboxModel::~SuggestionsComboboxModel() {}

void SuggestionsComboboxModel::AddItem(
    const std::string& key, const string16& item) {
  items_.push_back(std::make_pair(key, item));
}

std::string SuggestionsComboboxModel::GetItemKeyAt(int index) const {
  return items_[index].first;
}

int SuggestionsComboboxModel::GetItemCount() const {
  return items_.size();
}

string16 SuggestionsComboboxModel::GetItemAt(int index) {
  return items_[index].second;
}

// MonthComboboxModel ----------------------------------------------------------

MonthComboboxModel::MonthComboboxModel() {}

MonthComboboxModel::~MonthComboboxModel() {}

int MonthComboboxModel::GetItemCount() const {
  return 12;
}

string16 MonthComboboxModel::GetItemAt(int index) {
  return ASCIIToUTF16(StringPrintf("%2d", index + 1));
}

// YearComboboxModel -----------------------------------------------------------

YearComboboxModel::YearComboboxModel() : this_year_(0) {
  base::Time time = base::Time::Now();
  base::Time::Exploded exploded;
  time.LocalExplode(&exploded);
  this_year_ = exploded.year;
}

YearComboboxModel::~YearComboboxModel() {}

int YearComboboxModel::GetItemCount() const {
  return 10;
}

string16 YearComboboxModel::GetItemAt(int index) {
  return base::IntToString16(this_year_ + index);
}

}  // autofill
