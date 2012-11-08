// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_VIEW_H_

namespace autofill {

class AutofillDialogController;

// An interface for the dialog that appears when a site initiates an Autofill
// action via the imperative autocomplete API.
class AutofillDialogView {
 public:
  virtual ~AutofillDialogView();

  // Shows the dialog.
  virtual void Show() = 0;

  // Factory function to create the dialog (implemented once per view
  // implementation). |controller| will own the created dialog.
  static AutofillDialogView* Create(AutofillDialogController* controller);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_VIEW_H_
