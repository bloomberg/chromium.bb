// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_ACCOUNT_CHOOSER_MODEL_H_
#define CHROME_BROWSER_UI_AUTOFILL_ACCOUNT_CHOOSER_MODEL_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/strings/string16.h"
#include "components/autofill/core/browser/autofill_manager_delegate.h"
#include "ui/base/models/simple_menu_model.h"

class AutofillMetrics;
class Profile;

namespace autofill {

class AccountChooserModel;

// A delegate interface to allow the AccountChooserModel to inform its owner
// of changes.
class AccountChooserModelDelegate {
 public:
  virtual ~AccountChooserModelDelegate();

  // Called right before the account chooser is shown.
  virtual void AccountChooserWillShow() = 0;

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
                      Profile* profile,
                      const AutofillMetrics& metric_logger);
  virtual ~AccountChooserModel();

  // ui::SimpleMenuModel implementation.
  virtual void MenuWillShow() OVERRIDE;

  // ui::SimpleMenuModel::Delegate implementation.
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;
  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE;
  virtual void MenuWillShow(ui::SimpleMenuModel* source) OVERRIDE;

  // Sets the selection to the currently active Online Wallet account.
  // Should be called if the user attempts to sign into the Online Wallet
  // (e.g. when the user clicks the "Sign-in" link).
  void SelectActiveWalletAccount();

  // Sets the selection to the local Autofill data.
  void SelectUseAutofill();

  // Returns true if there are any accounts for the user to choose from.
  bool HasAccountsToChoose() const;

  // Sets the name of the accounts used to communicate with the Online Wallet.
  void SetWalletAccounts(const std::vector<std::string>& accounts);

  // Clears the set of accounts used to communicate with the Online Wallet.
  // Any Wallet error automatically clears the currently active account name.
  void ClearWalletAccounts();

  // Returns the name of the currently active account, or an empty string.
  // The currently active account may not be the checked item in the menu, but
  // will be the most recently checked wallet account item.
  base::string16 GetActiveWalletAccountName() const;

  // Returns the index of the account that is currently active.
  size_t GetActiveWalletAccountIndex() const;

  // Disables all Wallet accounts and switches to the local Autofill data.
  // Should be called when the Wallet server returns an error.
  void SetHadWalletError();

  // Switches the dialog to the local Autofill data.
  // Should be called when the Online Wallet sign-in attempt has failed.
  void SetHadWalletSigninError();

  // Returns true if the selected account is an Online Wallet account.
  bool WalletIsSelected() const;

 protected:
  // Command IDs of the items in this menu; protected for the tests.
  // kAutofillItemId is "Pay without the Wallet" (local autofill data).
  static const int kAutofillItemId;
  // Wallet account menu item IDs are this value + the account index.
  static const int kWalletAccountsStartId;

 private:
  // Reconstructs the set of menu items.
  void ReconstructMenuItems();

  AccountChooserModelDelegate* delegate_;

  // The command id of the currently selected item.
  int checked_item_;

  // The index of the active wallet account.
  size_t active_wallet_account_;

  // Whether there has been a Wallet error.
  bool had_wallet_error_;

  // For logging UMA metrics.
  const AutofillMetrics& metric_logger_;

  // The names (emails) of the signed in accounts, gotten from the
  // Online Wallet service.
  std::vector<std::string> wallet_accounts_;

  DISALLOW_COPY_AND_ASSIGN(AccountChooserModel);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_ACCOUNT_CHOOSER_MODEL_H_
