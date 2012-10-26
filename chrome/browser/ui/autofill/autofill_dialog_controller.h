// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_CONTROLLER_H_

#include "base/memory/scoped_ptr.h"
#include "base/string16.h"

namespace content {
class WebContents;
}

class AutofillDialogView;

// This class drives the dialog that appears when a site uses the imperative
// autocomplete API to fill out a form.
class AutofillDialogController {
 public:
  enum Action {
    AUTOFILL_ACTION_ABORT,
    AUTOFILL_ACTION_SUBMIT,
  };

  explicit AutofillDialogController(content::WebContents* contents);
  ~AutofillDialogController();

  void Show();

  // Called by the view.
  string16 DialogTitle() const;
  string16 CancelButtonText() const;
  string16 ConfirmButtonText() const;
  bool ConfirmButtonEnabled() const;

  // Called when the view has been closed. The value for |action| indicates
  // whether the Autofill operation should be aborted.
  void ViewClosed(Action action);

  content::WebContents* web_contents() { return contents_; }

 private:
  // The WebContents where the Autofill action originated.
  content::WebContents* const contents_;

  scoped_ptr<AutofillDialogView> view_;

  DISALLOW_COPY_AND_ASSIGN(AutofillDialogController);
};

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_CONTROLLER_H_
