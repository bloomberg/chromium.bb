// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_VIEW_H_

#include "chrome/browser/ui/autofill/autofill_dialog_controller.h"

namespace autofill {

// An interface for the dialog that appears when a site initiates an Autofill
// action via the imperative autocomplete API.
class AutofillDialogView {
 public:
  virtual ~AutofillDialogView();

  // Shows the dialog.
  virtual void Show() = 0;

  // Called when the contents of a section have changed.
  virtual void UpdateSection(DialogSection section) = 0;

  // Fills |output| with data the user manually input.
  virtual void GetUserInput(DialogSection section, DetailOutputMap* output) = 0;

  // Returns the state of the "use billing address for shipping" checkbox.
  virtual bool UseBillingForShipping() = 0;

  // Factory function to create the dialog (implemented once per view
  // implementation). |controller| will own the created dialog.
  static AutofillDialogView* Create(AutofillDialogController* controller);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_VIEW_H_
