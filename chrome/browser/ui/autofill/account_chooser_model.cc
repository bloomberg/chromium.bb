// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/account_chooser_model.h"

#include "base/bind.h"
#include "base/prefs/pref_service.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/pref_names.h"
#include "components/autofill/browser/autofill_metrics.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

namespace autofill {

const int AccountChooserModel::kActiveWalletItemId = 0;
const int AccountChooserModel::kAutofillItemId = 1;

AccountChooserModelDelegate::~AccountChooserModelDelegate() {}

AccountChooserModel::AccountChooserModel(
    AccountChooserModelDelegate* delegate,
    PrefService* prefs,
    const AutofillMetrics& metric_logger,
    DialogType dialog_type)
    : ui::SimpleMenuModel(this),
      delegate_(delegate),
      checked_item_(
          prefs->GetBoolean(::prefs::kAutofillDialogPayWithoutWallet) ?
              kAutofillItemId : kActiveWalletItemId),
      had_wallet_error_(false),
      metric_logger_(metric_logger),
      dialog_type_(dialog_type) {
  ReconstructMenuItems();
}

AccountChooserModel::~AccountChooserModel() {
}

void AccountChooserModel::SelectActiveWalletAccount() {
  ExecuteCommand(kActiveWalletItemId, 0);
}

void AccountChooserModel::SelectUseAutofill() {
  ExecuteCommand(kAutofillItemId, 0);
}

bool AccountChooserModel::HasAccountsToChoose() const {
  return !active_wallet_account_name_.empty();
}

void AccountChooserModel::SetActiveWalletAccountName(
    const string16& account) {
  active_wallet_account_name_ = account;
  ReconstructMenuItems();
  delegate_->UpdateAccountChooserView();
}

void AccountChooserModel::ClearActiveWalletAccountName() {
  active_wallet_account_name_.clear();
  ReconstructMenuItems();
  delegate_->UpdateAccountChooserView();
}

bool AccountChooserModel::IsCommandIdChecked(int command_id) const {
  return command_id == checked_item_;
}

bool AccountChooserModel::IsCommandIdEnabled(int command_id) const {
  // Currently, _any_ (non-sign-in) error disables _all_ Wallet accounts.
  if (command_id != kAutofillItemId && had_wallet_error_)
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
  ReconstructMenuItems();
  delegate_->AccountChoiceChanged();
}

void AccountChooserModel::SetHadWalletError() {
  // Any non-sign-in error disables all Wallet accounts.
  had_wallet_error_ = true;
  ClearActiveWalletAccountName();
  ExecuteCommand(kAutofillItemId, 0);
}

void AccountChooserModel::SetHadWalletSigninError() {
  ClearActiveWalletAccountName();
  ExecuteCommand(kAutofillItemId, 0);
}

bool AccountChooserModel::WalletIsSelected() const {
  return checked_item_ != kAutofillItemId;
}

bool AccountChooserModel::IsActiveWalletAccountSelected() const {
  return checked_item_ == kActiveWalletItemId;
}

void AccountChooserModel::ReconstructMenuItems() {
  Clear();
  const gfx::Image& wallet_icon =
      ui::ResourceBundle::GetSharedInstance().GetImageNamed(IDR_WALLET_ICON);

  if (!active_wallet_account_name_.empty()) {
    AddCheckItem(kActiveWalletItemId, active_wallet_account_name_);
    SetIcon(GetIndexOfCommandId(kActiveWalletItemId), wallet_icon);
  } else if (checked_item_ == kActiveWalletItemId) {
    // A selected active Wallet account with an empty account name means
    // that the sign-in attempt is in progress.
    // TODO(aruslan): http://crbug.com/230932
    // A throbber should be shown until the Wallet account name is set.
    AddCheckItem(kActiveWalletItemId,
                 l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_GOOGLE_WALLET));
  }

  AddCheckItemWithStringId(kAutofillItemId,
                           IDS_AUTOFILL_DIALOG_PAY_WITHOUT_WALLET);
}

}  // namespace autofill
