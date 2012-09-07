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
  AutofillExternalDelegateViews(TabContents* tab_contents,
                                AutofillManager* autofill_manager);

  virtual ~AutofillExternalDelegateViews();

  // Used by the popup view to indicate it has been destroyed.
  void InvalidateView();

 protected:
  AutofillPopupViewViews* popup_view() { return popup_view_; }

  // AutofillExternalDelegate implementation.
  // Make protected to allow tests to override.
  virtual void HideAutofillPopupInternal() OVERRIDE;

 private:
  // AutofillExternalDelegate implementation.
  virtual void OnQueryPlatformSpecific(
      int query_id,
      const webkit::forms::FormData& form,
      const webkit::forms::FormField& field,
      const gfx::Rect& bounds) OVERRIDE;
  virtual void ApplyAutofillSuggestions(
      const std::vector<string16>& autofill_values,
      const std::vector<string16>& autofill_labels,
      const std::vector<string16>& autofill_icons,
      const std::vector<int>& autofill_unique_ids) OVERRIDE;
  virtual void SetBounds(const gfx::Rect& bounds) OVERRIDE;

  // Create a valid view to display the autofill results if one doesn't
  // currently exist.
  void CreateViewIfNeeded();

  // We use a raw pointer here because even though we usually create the view,
  // its widget handles deleting it.
  AutofillPopupViewViews* popup_view_;

  DISALLOW_COPY_AND_ASSIGN(AutofillExternalDelegateViews);
};

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_EXTERNAL_DELEGATE_VIEWS_H_
