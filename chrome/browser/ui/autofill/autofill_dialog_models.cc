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
#include "components/autofill/browser/autofill_metrics.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace autofill {

SuggestionsMenuModelDelegate::~SuggestionsMenuModelDelegate() {}

// SuggestionsMenuModel ----------------------------------------------------

SuggestionsMenuModel::SuggestionsMenuModel(
    SuggestionsMenuModelDelegate* delegate)
    : ALLOW_THIS_IN_INITIALIZER_LIST(ui::SimpleMenuModel(this)),
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
  checked_item_ = command_id;
  delegate_->SuggestionItemSelected(*this);
}

// AccountChooserModel ---------------------------------------------------------

const int AccountChooserModel::kWalletItemId = 0;
const int AccountChooserModel::kAutofillItemId = 1;

AccountChooserModelDelegate::~AccountChooserModelDelegate() {}

AccountChooserModel::AccountChooserModel(
    AccountChooserModelDelegate* delegate,
    PrefService* prefs,
    const AutofillMetrics& metric_logger,
    DialogType dialog_type)
    : ALLOW_THIS_IN_INITIALIZER_LIST(ui::SimpleMenuModel(this)),
      account_delegate_(delegate),
      checked_item_(
          prefs->GetBoolean(::prefs::kAutofillDialogPayWithoutWallet) ?
          kAutofillItemId : kWalletItemId),
      had_wallet_error_(false),
      metric_logger_(metric_logger),
      dialog_type_(dialog_type) {
  AddCheckItem(kWalletItemId,
               l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_GOOGLE_WALLET));
  SetIcon(
      kWalletItemId,
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(IDR_WALLET_ICON));
  AddCheckItemWithStringId(kAutofillItemId,
                           IDS_AUTOFILL_DIALOG_PAY_WITHOUT_WALLET);
}

AccountChooserModel::~AccountChooserModel() {
}

bool AccountChooserModel::IsCommandIdChecked(int command_id) const {
  return command_id == checked_item_;
}

bool AccountChooserModel::IsCommandIdEnabled(int command_id) const {
  if (command_id == kWalletItemId && had_wallet_error_)
    return false;

  return true;
}

bool AccountChooserModel::GetAcceleratorForCommandId(
    int command_id,
    ui::Accelerator* accelerator) {
  return false;
}

void AccountChooserModel::ExecuteCommand(int command_id, int event_flags) {
  if (checked_item_ == command_id)
    return;

  // Log metrics.
  AutofillMetrics::DialogUiEvent chooser_event;
  if (command_id == kAutofillItemId) {
    chooser_event =
        AutofillMetrics::DIALOG_UI_ACCOUNT_CHOOSER_SWITCHED_TO_AUTOFILL;
  } else if (checked_item_ == kAutofillItemId) {
    chooser_event =
        AutofillMetrics::DIALOG_UI_ACCOUNT_CHOOSER_SWITCHED_TO_WALLET;
  } else {
    chooser_event =
        AutofillMetrics::DIALOG_UI_ACCOUNT_CHOOSER_SWITCHED_WALLET_ACCOUNT;
  }
  metric_logger_.LogDialogUiEvent(dialog_type_, chooser_event);

  checked_item_ = command_id;
  account_delegate_->AccountChoiceChanged();
}

void AccountChooserModel::SetHadWalletError() {
  had_wallet_error_ = true;
  ExecuteCommand(kAutofillItemId, 0);
}

void AccountChooserModel::SetHadWalletSigninError() {
  ExecuteCommand(kAutofillItemId, 0);
}

bool AccountChooserModel::WalletIsSelected() const {
  return checked_item_ == kWalletItemId;
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
