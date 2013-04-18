// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_ACCOUNT_CHOOSER_MODEL_H_
#define CHROME_BROWSER_UI_AUTOFILL_ACCOUNT_CHOOSER_MODEL_H_

#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/string16.h"
#include "components/autofill/browser/autofill_manager_delegate.h"
#include "ui/base/models/simple_menu_model.h"

class AutofillMetrics;
class PrefService;

namespace autofill {

// A delegate interface to allow the AccountChooserModel to inform its owner
// of changes.
class AccountChooserModelDelegate {
 public:
  virtual ~AccountChooserModelDelegate();

  // Called when the active account has changed.
  virtual void AccountChoiceChanged() = 0;

  // Called when the account chooser UI needs to be updated.
  virtual void UpdateAccountChooserView() = 0;
};

// A menu model for the account chooser. This allows users to switch between
// Online Wallet accounts and local Autofill data.
// Terminology:
// - "Active Wallet account": the account used for communications with the
// Online Wallet service. There may be multiple signed-in accounts, but at any
// point of time at most one of is active.
class AccountChooserModel : public ui::SimpleMenuModel,
                            public ui::SimpleMenuModel::Delegate {
 public:
  AccountChooserModel(AccountChooserModelDelegate* delegate,
                      PrefService* prefs,
                      const AutofillMetrics& metric_logger,
                      DialogType dialog_type);
  virtual ~AccountChooserModel();

  // ui::SimpleMenuModel::Delegate implementation.
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;
  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE;

  // Sets the selection to the currently active Online Wallet account.
  // Should be called if the user attempts to sign into the Online Wallet
  // (e.g. when the user clicks the "Sign-in" link).
  void SelectActiveWalletAccount();

  // Sets the selection to the local Autofill data.
  void SelectUseAutofill();

  // Returns true if there are any accounts for the user to choose from.
  bool HasAccountsToChoose() const;

  // Sets the name of the account used to communicate with the Online Wallet.
  void SetActiveWalletAccountName(const string16& account);

  // Clears the name of the account used to communicate with the Online Wallet.
  // Any Wallet error automatically clears the currently active account name.
  void ClearActiveWalletAccountName();

  // Returns the name of the currently active account, or an empty string.
  const string16& active_wallet_account_name() const {
    return active_wallet_account_name_;
  }

  // Disables all Wallet accounts and switches to the local Autofill data.
  // Should be called when the Wallet server returns an error.
  void SetHadWalletError();

  // Switches the dialog to the local Autofill data.
  // Should be called when the Online Wallet sign-in attempt has failed.
  void SetHadWalletSigninError();

  bool had_wallet_error() const { return had_wallet_error_; }

  // Returns true if the selected account is an Online Wallet account.
  bool WalletIsSelected() const;

  // Returns true if the current selection matches the currently active
  // Wallet account.
  bool IsActiveWalletAccountSelected() const;

  // Returns the command id of the current selection.
  int checked_item() const { return checked_item_; }

 protected:
  // Command IDs of the items in this menu; protected for the tests.
  // kActiveWalletItemId is the currently active account.
  // kAutofillItemId is "Pay without the Wallet" (local autofill data).
  // In the future, kFirstAdditionalItemId will be added as the first id
  // for additional accounts.
  static const int kActiveWalletItemId;
  static const int kAutofillItemId;

 private:
  // Reconstructs the set of menu items.
  void ReconstructMenuItems();

  AccountChooserModelDelegate* delegate_;

  // The command id of the currently selected item.
  int checked_item_;

  // Whether there has been a Wallet error while the owning dialog has been
  // open.
  bool had_wallet_error_;

  // For logging UMA metrics.
  const AutofillMetrics& metric_logger_;
  const DialogType dialog_type_;

  // The name (email) of the account currently used in communications with the
  // Online Wallet service.
  string16 active_wallet_account_name_;

  DISALLOW_COPY_AND_ASSIGN(AccountChooserModel);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_ACCOUNT_CHOOSER_MODEL_H_
