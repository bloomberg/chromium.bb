// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_DIALOG_VIEW_TESTER_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_DIALOG_VIEW_TESTER_VIEWS_H_

#include "base/basictypes.h"
#include "chrome/browser/ui/autofill/autofill_dialog_view_tester.h"

namespace autofill {

class AutofillDialogViews;

class AutofillDialogViewTesterViews : public AutofillDialogViewTester {
 public:
  explicit AutofillDialogViewTesterViews(AutofillDialogViews* view);
  virtual ~AutofillDialogViewTesterViews();

  // TestableAutofillDialogView implementation:
  virtual void SubmitForTesting() OVERRIDE;
  virtual void CancelForTesting() OVERRIDE;
  virtual base::string16 GetTextContentsOfInput(ServerFieldType type) OVERRIDE;
  virtual void SetTextContentsOfInput(ServerFieldType type,
                                      const base::string16& contents) OVERRIDE;
  virtual void SetTextContentsOfSuggestionInput(
      DialogSection section,
      const base::string16& text) OVERRIDE;
  virtual void ActivateInput(ServerFieldType type) OVERRIDE;
  virtual gfx::Size GetSize() const OVERRIDE;
  virtual content::WebContents* GetSignInWebContents() OVERRIDE;
  virtual bool IsShowingOverlay() const OVERRIDE;
  virtual bool IsShowingSection(DialogSection section) const OVERRIDE;

 private:
  AutofillDialogViews* view_;

  DISALLOW_COPY_AND_ASSIGN(AutofillDialogViewTesterViews);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_DIALOG_VIEW_TESTER_VIEWS_H_
