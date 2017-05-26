// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_dialog_models.h"

#include "base/strings/string16.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill {

namespace {

// Number of years to be shown in the year combobox, including the current year.
// YearComboboxModel has the option of passing an additional year if not
// contained within the initial range.
const int kNumberOfExpirationYears = 10;

// Returns the items that are in the expiration year dropdown. If
// |additional_year| is not 0 and not within the normal range, it will be added
// accordingly.
std::vector<base::string16> GetExpirationYearItems(int additional_year) {
  std::vector<base::string16> years;
  // Add the "Year" placeholder item.
  years.push_back(
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_PLACEHOLDER_EXPIRY_YEAR));

  base::Time::Exploded now_exploded;
  AutofillClock::Now().LocalExplode(&now_exploded);

  if (additional_year != 0 && additional_year < now_exploded.year)
    years.push_back(base::UTF8ToUTF16(std::to_string(additional_year)));

  for (int i = 0; i < kNumberOfExpirationYears; i++)
    years.push_back(base::UTF8ToUTF16(std::to_string(now_exploded.year + i)));

  if (additional_year != 0 &&
      additional_year >= now_exploded.year + kNumberOfExpirationYears) {
    years.push_back(base::UTF8ToUTF16(std::to_string(additional_year)));
  }

  return years;
}

// Formats a month, zero-padded (e.g. "02").
base::string16 FormatMonth(int month) {
  return base::ASCIIToUTF16(base::StringPrintf("%.2d", month));
}

}  // namespace

SuggestionsMenuModelDelegate::~SuggestionsMenuModelDelegate() {}

// SuggestionsMenuModel ----------------------------------------------------

SuggestionsMenuModel::SuggestionsMenuModel(
    SuggestionsMenuModelDelegate* delegate)
    : ui::SimpleMenuModel(this),
      delegate_(delegate),
      checked_item_(0) {}

SuggestionsMenuModel::~SuggestionsMenuModel() {}

void SuggestionsMenuModel::AddKeyedItem(
    const std::string& key, const base::string16& display_label) {
  Item item = { key, true };
  items_.push_back(item);
  AddCheckItem(items_.size() - 1, display_label);
}

void SuggestionsMenuModel::AddKeyedItemWithIcon(
    const std::string& key,
    const base::string16& display_label,
    const gfx::Image& icon) {
  AddKeyedItem(key, display_label);
  SetIcon(items_.size() - 1, icon);
}

void SuggestionsMenuModel::AddKeyedItemWithMinorText(
    const std::string& key,
    const base::string16& display_label,
    const base::string16& display_minor_text) {
  AddKeyedItem(key, display_label);
  SetMinorText(items_.size() - 1, display_minor_text);
}

void SuggestionsMenuModel::AddKeyedItemWithMinorTextAndIcon(
    const std::string& key,
    const base::string16& display_label,
    const base::string16& display_minor_text,
    const gfx::Image& icon) {
  AddKeyedItemWithIcon(key, display_label, icon);
  SetMinorText(items_.size() - 1, display_minor_text);
}

void SuggestionsMenuModel::Reset() {
  checked_item_ = 0;
  items_.clear();
  Clear();
}

std::string SuggestionsMenuModel::GetItemKeyAt(int index) const {
  return items_[index].key;
}

std::string SuggestionsMenuModel::GetItemKeyForCheckedItem() const {
  if (items_.empty())
    return std::string();

  return items_[checked_item_].key;
}

void SuggestionsMenuModel::SetCheckedItem(const std::string& item_key) {
  for (size_t i = 0; i < items_.size(); ++i) {
    if (items_[i].key == item_key) {
      if (IsEnabledAt(i))
        checked_item_ = i;
      break;
    }
  }
}

void SuggestionsMenuModel::SetCheckedIndex(size_t index) {
  DCHECK_LT(index, items_.size());
  checked_item_ = index;
}

void SuggestionsMenuModel::SetEnabled(const std::string& item_key,
                                      bool enabled) {
  items_[GetItemIndex(item_key)].enabled = enabled;
}

bool SuggestionsMenuModel::IsCommandIdChecked(
    int command_id) const {
  return checked_item_ == command_id;
}

bool SuggestionsMenuModel::IsCommandIdEnabled(
    int command_id) const {
  // Please note: command_id is same as the 0-based index in |items_|.
  DCHECK_GE(command_id, 0);
  DCHECK_LT(static_cast<size_t>(command_id), items_.size());
  return items_[command_id].enabled;
}

void SuggestionsMenuModel::ExecuteCommand(int command_id, int event_flags) {
  delegate_->SuggestionItemSelected(this, command_id);
}

size_t SuggestionsMenuModel::GetItemIndex(const std::string& item_key) {
  for (size_t i = 0; i < items_.size(); ++i) {
    if (items_[i].key == item_key)
      return i;
  }

  NOTREACHED();
  return 0;
}

// MonthComboboxModel ----------------------------------------------------------

MonthComboboxModel::MonthComboboxModel() {}

MonthComboboxModel::~MonthComboboxModel() {}

int MonthComboboxModel::GetItemCount() const {
  // 12 months plus the empty entry.
  return 13;
}

base::string16 MonthComboboxModel::GetItemAt(int index) {
  return index == 0 ?
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_PLACEHOLDER_EXPIRY_MONTH) :
      FormatMonth(index);
}

// YearComboboxModel -----------------------------------------------------------

YearComboboxModel::YearComboboxModel(int additional_year)
    : ui::SimpleComboboxModel(GetExpirationYearItems(additional_year)) {}

YearComboboxModel::~YearComboboxModel() {}

}  // namespace autofill
