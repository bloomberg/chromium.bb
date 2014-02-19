// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_VIEW_TESTER_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_VIEW_TESTER_H_

#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "chrome/browser/ui/autofill/autofill_dialog_types.h"
#include "components/autofill/core/browser/field_types.h"
#include "ui/gfx/size.h"

namespace content {
class WebContents;
}

namespace autofill {

class AutofillDialogView;

// Functionality that helps to test an AutofillDialogView.
class AutofillDialogViewTester {
 public:
  // Gets a AutofillDialogViewTester for |view|.
  static scoped_ptr<AutofillDialogViewTester> For(AutofillDialogView* view);

  virtual ~AutofillDialogViewTester() {}

  // Simulates the user pressing 'Submit' to accept the dialog.
  virtual void SubmitForTesting() = 0;

  // Simulates the user pressing 'Cancel' to abort the dialog.
  virtual void CancelForTesting() = 0;

  // Returns the actual contents of the input of |type|.
  virtual base::string16 GetTextContentsOfInput(ServerFieldType type) = 0;

  // Sets the actual contents of the input of |type|.
  virtual void SetTextContentsOfInput(ServerFieldType type,
                                      const base::string16& contents) = 0;

  // Sets the content of the extra field for a section.
  virtual void SetTextContentsOfSuggestionInput(DialogSection section,
                                                const base::string16& text) = 0;

  // Simulates a user activation of the input of |type|.
  virtual void ActivateInput(ServerFieldType type) = 0;

  // Get the size of the entire view.
  virtual gfx::Size GetSize() const = 0;

  // Get the web contents used to sign in to Google.
  virtual content::WebContents* GetSignInWebContents() = 0;

  // Whether the overlay is visible.
  virtual bool IsShowingOverlay() const = 0;

  // Whether |section| is currently showing.
  virtual bool IsShowingSection(DialogSection section) const = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_DIALOG_VIEW_TESTER_H_
