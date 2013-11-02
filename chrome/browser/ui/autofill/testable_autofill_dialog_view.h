// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_TESTABLE_AUTOFILL_DIALOG_VIEW_H_
#define CHROME_BROWSER_UI_AUTOFILL_TESTABLE_AUTOFILL_DIALOG_VIEW_H_

namespace autofill {

// Functions that an AutofillDialogView implementation should implement in order
// to assist in unit testing.
class TestableAutofillDialogView {
 public:
  virtual ~TestableAutofillDialogView() {}

  // Simulates the user pressing 'Submit' to accept the dialog.
  virtual void SubmitForTesting() = 0;

  // Simulates the user pressing 'Cancel' to abort the dialog.
  virtual void CancelForTesting() = 0;

  // Returns the actual contents of the input which is modelled by |input|.
  virtual string16 GetTextContentsOfInput(const DetailInput& input) = 0;

  // Sets the actual contents of the input which is modelled by |input|.
  virtual void SetTextContentsOfInput(const DetailInput& input,
                                      const string16& contents) = 0;

  // Sets the content of the extra field for a section.
  virtual void SetTextContentsOfSuggestionInput(DialogSection section,
                                                const base::string16& text) = 0;

  // Simulates a user activatino of the input which is modelled by |input|.
  virtual void ActivateInput(const DetailInput& input) = 0;

  // Get the size of the entire view.
  virtual gfx::Size GetSize() const = 0;

  // Get the web contents used to sign in to Google.
  virtual content::WebContents* GetSignInWebContents() = 0;

  // Whether the overlay is visible.
  virtual bool IsShowingOverlay() const = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_TESTABLE_AUTOFILL_DIALOG_VIEW_H_
