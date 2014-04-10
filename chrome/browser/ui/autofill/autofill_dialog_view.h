// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_VIEW_H_

#include "chrome/browser/ui/autofill/autofill_dialog_view_delegate.h"

namespace content {
class NavigationController;
}

namespace gfx {
class Size;
}

namespace autofill {

class AutofillDialogViewDelegate;

// An interface for the dialog that appears when a site initiates an Autofill
// action via the imperative autocomplete API.
class AutofillDialogView {
 public:
  virtual ~AutofillDialogView();

  // Shows the dialog.
  virtual void Show() = 0;

  // Closes the dialog window. May self-delete.
  virtual void Hide() = 0;

  // A hint that the view is going to receive a series of Update* calls soon,
  // and may want to delay visible changes until after the updates are over.
  // As multiple calls to UpdatesStarted may be stacked, and the view should
  // expect an equal number of calls to UpdateFinished().
  virtual void UpdatesStarted() = 0;

  // The matching call to UpdatesStarted.
  virtual void UpdatesFinished() = 0;

  // Called when a different notification is available.
  virtual void UpdateNotificationArea() = 0;

  // Called when account details may have changed (user logs in to GAIA, creates
  // a new account, etc.).
  virtual void UpdateAccountChooser() = 0;

  // Updates the button strip based on the current controller state.
  virtual void UpdateButtonStrip() = 0;

  // Updates the dialog overlay in response to a change of state or animation
  // progression.
  virtual void UpdateOverlay() = 0;

  // Updates the container for the detail inputs.
  virtual void UpdateDetailArea() = 0;

  // Updates the validity status of the detail inputs.
  virtual void UpdateForErrors() = 0;

  // Called when the contents of a section have changed.
  virtual void UpdateSection(DialogSection section) = 0;

  // Updates the error bubble for this view.
  virtual void UpdateErrorBubble() = 0;

  // Fills the given section with Autofill data that was triggered by a user
  // interaction with |originating_input|.
  virtual void FillSection(DialogSection section,
                           ServerFieldType originating_type) = 0;

  // Fills |output| with data the user manually input.
  virtual void GetUserInput(DialogSection section, FieldValueMap* output) = 0;

  // Gets the CVC value the user typed to go along with the stored credit card
  // data. If the user is inputing credit card data from scratch, this is not
  // relevant.
  virtual base::string16 GetCvc() = 0;

  // Returns true if new or edited autofill details should be saved.
  virtual bool SaveDetailsLocally() = 0;

  // Triggers dialog to sign in to Google.
  // Returns a NotificationSource to be used to monitor for sign-in completion.
  virtual const content::NavigationController* ShowSignIn() = 0;

  // Closes out any sign-in UI and returns to normal operation.
  virtual void HideSignIn() = 0;

  // Called when the active suggestions data model changed.
  virtual void ModelChanged() = 0;

  // Called by AutofillDialogSignInDelegate when the sign-in page experiences a
  // resize. |pref_size| is the new preferred size of the sign-in page.
  virtual void OnSignInResize(const gfx::Size& pref_size) = 0;

  // Tells the view to validate its manual input in |section|.
  virtual void ValidateSection(DialogSection section) = 0;

  // Factory function to create the dialog (implemented once per view
  // implementation). |controller| will own the created dialog.
  static AutofillDialogView* Create(AutofillDialogViewDelegate* delegate);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_VIEW_H_
