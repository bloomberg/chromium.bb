// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_dialog_models.h"

#include "base/bind.h"
#include "base/prefs/pref_service.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/pref_names.h"
#include "components/autofill/browser/autofill_country.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace autofill {

SuggestionsMenuModelDelegate::~SuggestionsMenuModelDelegate() {}

// SuggestionsMenuModel ----------------------------------------------------

SuggestionsMenuModel::SuggestionsMenuModel(
    SuggestionsMenuModelDelegate* delegate)
    : ui::SimpleMenuModel(this),
      delegate_(delegate),
      checked_item_(0) {}

SuggestionsMenuModel::~SuggestionsMenuModel() {}

void SuggestionsMenuModel::AddKeyedItem(
    const std::string& key, const string16& item) {
  items_.push_back(std::make_pair(key, item));
  AddCheckItem(items_.size() - 1, item);
}

void SuggestionsMenuModel::AddKeyedItemWithIcon(
    const std::string& key, const string16& item, const gfx::Image& icon) {
  AddKeyedItem(key, item);
  SetIcon(items_.size() - 1, icon);
}

void SuggestionsMenuModel::AddKeyedItemWithSublabel(
    const std::string& key,
    const string16& display_label, const string16& display_sublabel) {
  AddKeyedItem(key, display_label);
  SetSublabel(items_.size() - 1, display_sublabel);
}

void SuggestionsMenuModel::AddKeyedItemWithSublabelAndIcon(
    const std::string& key,
    const string16& display_label, const string16& display_sublabel,
    const gfx::Image& icon) {
  AddKeyedItemWithIcon(key, display_label, icon);
  SetSublabel(items_.size() - 1, display_sublabel);
}

void SuggestionsMenuModel::Reset() {
  checked_item_ = 0;
  items_.clear();
  Clear();
}

std::string SuggestionsMenuModel::GetItemKeyAt(int index) const {
  return items_[index].first;
}

std::string SuggestionsMenuModel::GetItemKeyForCheckedItem() const {
  if (items_.empty())
    return std::string();

  return items_[checked_item_].first;
}

void SuggestionsMenuModel::SetCheckedItem(const std::string& item_key) {
  for (size_t i = 0; i < items_.size(); ++i) {
    if (items_[i].first == item_key) {
      checked_item_ = i;
      return;
    }
  }

  NOTREACHED();
}

void SuggestionsMenuModel::SetCheckedIndex(size_t index) {
  DCHECK_LE(index, items_.size());
  checked_item_ = index;
}

bool SuggestionsMenuModel::IsCommandIdChecked(
    int command_id) const {
  return checked_item_ == command_id;
}

bool SuggestionsMenuModel::IsCommandIdEnabled(
    int command_id) const {
  return true;
}

bool SuggestionsMenuModel::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) {
  return false;
}

void SuggestionsMenuModel::ExecuteCommand(int command_id, int event_flags) {
  delegate_->SuggestionItemSelected(this, command_id);
}

// MonthComboboxModel ----------------------------------------------------------

MonthComboboxModel::MonthComboboxModel() {}

MonthComboboxModel::~MonthComboboxModel() {}

int MonthComboboxModel::GetItemCount() const {
  // 12 months plus the empty entry.
  return 13;
}

// static
string16 MonthComboboxModel::FormatMonth(int index) {
  return ASCIIToUTF16(base::StringPrintf("%.2d", index));
}

string16 MonthComboboxModel::GetItemAt(int index) {
  return index == 0 ? string16() : FormatMonth(index);
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
  // 10 years plus the empty entry.
  return 11;
}

string16 YearComboboxModel::GetItemAt(int index) {
  if (index == 0)
    return string16();

  return base::IntToString16(this_year_ + index - 1);
}

}  // autofill
