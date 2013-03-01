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
#include "chrome/browser/autofill/autofill_country.h"
#include "chrome/common/pref_names.h"
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

void SuggestionsMenuModel::ExecuteCommand(int command_id) {
  checked_item_ = command_id;
  delegate_->SuggestionItemSelected(*this);
}

// AccountChooserModel ---------------------------------------------------------

const int AccountChooserModel::kWalletItemId = 0;
const int AccountChooserModel::kAutofillItemId = 1;

AccountChooserModelDelegate::~AccountChooserModelDelegate() {}

AccountChooserModel::AccountChooserModel(
    AccountChooserModelDelegate* delegate,
    PrefService* prefs)
    : ALLOW_THIS_IN_INITIALIZER_LIST(ui::SimpleMenuModel(this)),
      account_delegate_(delegate),
      prefs_(prefs),
      checked_item_(kWalletItemId),
      had_wallet_error_(false) {
  pref_change_registrar_.Init(prefs);
  pref_change_registrar_.Add(
      prefs::kAutofillDialogPayWithoutWallet,
      base::Bind(&AccountChooserModel::PrefChanged, base::Unretained(this)));

  // TODO(estade): proper strings and l10n.
  AddCheckItem(kWalletItemId, ASCIIToUTF16("Google Wallet"));
  // TODO(estade): icons on check items are not yet supported in Views.
  SetIcon(
      kWalletItemId,
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(IDR_WALLET_ICON));
  AddCheckItemWithStringId(kAutofillItemId,
                           IDS_AUTOFILL_DIALOG_PAY_WITHOUT_WALLET);
  UpdateCheckmarkFromPref();
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

void AccountChooserModel::ExecuteCommand(int command_id) {
  if (checked_item_ == command_id)
    return;

  checked_item_ = command_id;
  account_delegate_->AccountChoiceChanged();
}

void AccountChooserModel::SetHadWalletError() {
  had_wallet_error_ = true;
  checked_item_ = kAutofillItemId;
  account_delegate_->AccountChoiceChanged();
}

bool AccountChooserModel::WalletIsSelected() const {
  return checked_item_ == 0;
}

void AccountChooserModel::PrefChanged(const std::string& pref) {
  DCHECK(pref == prefs::kAutofillDialogPayWithoutWallet);
  UpdateCheckmarkFromPref();
  account_delegate_->AccountChoiceChanged();
}

void AccountChooserModel::UpdateCheckmarkFromPref() {
  if (prefs_->GetBoolean(prefs::kAutofillDialogPayWithoutWallet))
    checked_item_ = kAutofillItemId;
  else
    checked_item_ = kWalletItemId;
}

// MonthComboboxModel ----------------------------------------------------------

MonthComboboxModel::MonthComboboxModel() {}

MonthComboboxModel::~MonthComboboxModel() {}

int MonthComboboxModel::GetItemCount() const {
  // 12 months plus the empty entry.
  return 13;
}

string16 MonthComboboxModel::GetItemAt(int index) {
  if (index == 0)
    return string16();

  return ASCIIToUTF16(StringPrintf("%2d", index));
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
