// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_TEST_AUTOFILL_EXTERNAL_DELEGATE_H_
#define CHROME_BROWSER_AUTOFILL_TEST_AUTOFILL_EXTERNAL_DELEGATE_H_

#include "chrome/browser/autofill/autofill_external_delegate.h"

class AutofillManager;

namespace autofill {

// Calls the required functions on the given external delegate to cause the
// delegate to display a popup.
void GenerateTestAutofillPopup(
    AutofillExternalDelegate* autofill_external_delegate);

// This test class is meant to give tests a base AutofillExternalDelegate
// class that requires no additional work to compile with (i.e. all the
// pure virtual functions have been giving empty methods).
class TestAutofillExternalDelegate : public AutofillExternalDelegate {
 public:
  TestAutofillExternalDelegate(content::WebContents* web_contents,
                               AutofillManager* autofill_manager);
  virtual ~TestAutofillExternalDelegate();

  virtual void ApplyAutofillSuggestions(
      const std::vector<string16>& autofill_values,
      const std::vector<string16>& autofill_labels,
      const std::vector<string16>& autofill_icons,
      const std::vector<int>& autofill_unique_ids) OVERRIDE;

  virtual void HideAutofillPopup() OVERRIDE;

  virtual void EnsurePopupForElement(const gfx::Rect& element_bounds) OVERRIDE;

  virtual void ControllerDestroyed() OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestAutofillExternalDelegate);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_TEST_AUTOFILL_EXTERNAL_DELEGATE_H_
