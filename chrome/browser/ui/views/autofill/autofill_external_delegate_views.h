// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_EXTERNAL_DELEGATE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_EXTERNAL_DELEGATE_VIEWS_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/autofill/autofill_external_delegate.h"

class AutofillPopupViewViews;

// The View specific implementations of the AutofillExternalDelegate,
// handling the communication between the Autofill and Autocomplete
// selection and the popup that shows the values.
class AutofillExternalDelegateViews : public AutofillExternalDelegate {
 public:
  AutofillExternalDelegateViews(content::WebContents* web_contents,
                                AutofillManager* autofill_manager);

  virtual ~AutofillExternalDelegateViews();

  // Used by the popup view to indicate it has been destroyed.
  void InvalidateView();

 protected:
  AutofillPopupViewViews* popup_view() { return popup_view_; }

  // AutofillExternalDelegate implementation.
  // Make protected to allow tests to override.
  virtual void HideAutofillPopupInternal() OVERRIDE;
  virtual void CreatePopupForElement(const gfx::Rect& element_bounds) OVERRIDE;

 private:
  // AutofillExternalDelegate implementation.
  virtual void ApplyAutofillSuggestions(
      const std::vector<string16>& autofill_values,
      const std::vector<string16>& autofill_labels,
      const std::vector<string16>& autofill_icons,
      const std::vector<int>& autofill_unique_ids) OVERRIDE;

  // We use a raw pointer here because even though we usually create the view,
  // its widget handles deleting it (which means it may outlive this object).
  AutofillPopupViewViews* popup_view_;

  DISALLOW_COPY_AND_ASSIGN(AutofillExternalDelegateViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_EXTERNAL_DELEGATE_VIEWS_H_
