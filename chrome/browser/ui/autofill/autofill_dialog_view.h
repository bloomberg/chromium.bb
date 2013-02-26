// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_VIEW_H_

#include "chrome/browser/ui/autofill/autofill_dialog_controller.h"

namespace content {
class NavigationController;
}

namespace autofill {

// An interface for the dialog that appears when a site initiates an Autofill
// action via the imperative autocomplete API.
class AutofillDialogView {
 public:
  virtual ~AutofillDialogView();

  // Shows the dialog.
  virtual void Show() = 0;

  // Hides the dialog as if a user pressed cancel.
  virtual void Hide() = 0;

  // Called when a different notification is available.
  virtual void UpdateNotificationArea() = 0;

  // Called when account details may have changed (user logs in to GAIA, creates
  // a new account, etc.).
  virtual void UpdateAccountChooser() = 0;

  // Called when the contents of a section have changed.
  virtual void UpdateSection(DialogSection section) = 0;

  // Fills |output| with data the user manually input.
  virtual void GetUserInput(DialogSection section, DetailOutputMap* output) = 0;

  // Gets the CVC value the user typed to go along with the stored credit card
  // data. If the user is inputing credit card data from scratch, this is not
  // relevant.
  virtual string16 GetCvc() = 0;

  // Returns the state of the "use billing address for shipping" checkbox.
  virtual bool UseBillingForShipping() = 0;

  // Returns true if current input should be saved in Wallet (if it differs).
  virtual bool SaveDetailsInWallet() = 0;

  // Returns true if new or edited autofill details should be saved.
  virtual bool SaveDetailsLocally() = 0;

  // Triggers dialog to sign in to Google.
  // Returns a NotificationSource to be used to monitor for sign-in completion.
  virtual const content::NavigationController& ShowSignIn() = 0;

  // Closes out any signin UI and returns to normal operation.
  virtual void HideSignIn() = 0;

  // Updates the progress bar based on the Autocheckout progress. |value| should
  // be in [0.0, 1.0].
  virtual void UpdateProgressBar(double value) = 0;

  // Called when the active suggestions data model changed.
  virtual void ModelChanged() = 0;

  // Simulates the user pressing 'Submit' to accept the dialog.
  virtual void SubmitForTesting() = 0;

  // Simulates the user pressing 'Cancel' to abort the dialog.
  virtual void CancelForTesting() = 0;

  // Factory function to create the dialog (implemented once per view
  // implementation). |controller| will own the created dialog.
  static AutofillDialogView* Create(AutofillDialogController* controller);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_VIEW_H_
