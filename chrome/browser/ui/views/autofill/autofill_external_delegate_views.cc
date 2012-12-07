// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_external_delegate_views.h"

#include "chrome/browser/ui/views/autofill/autofill_popup_view_views.h"

void AutofillExternalDelegate::CreateForWebContentsAndManager(
    content::WebContents* web_contents,
    AutofillManager* autofill_manager) {
  if (FromWebContents(web_contents))
    return;

  web_contents->SetUserData(
      UserDataKey(),
      new AutofillExternalDelegateViews(web_contents, autofill_manager));
}

AutofillExternalDelegateViews::AutofillExternalDelegateViews(
    content::WebContents* web_contents,
    AutofillManager* autofill_manager)
    : AutofillExternalDelegate(web_contents, autofill_manager),
      popup_view_(NULL) {
}

AutofillExternalDelegateViews::~AutofillExternalDelegateViews() {
  if (popup_view_) {
    popup_view_->ClearExternalDelegate();
  }
}

void AutofillExternalDelegateViews::InvalidateView() {
  popup_view_ = NULL;
  HideAutofillPopup();
}

void AutofillExternalDelegateViews::HideAutofillPopupInternal() {
  if (!popup_view_)
    return;

  popup_view_->Hide();
  popup_view_ = NULL;
}

void AutofillExternalDelegateViews::CreatePopupForElement(
    const gfx::Rect& element_bounds) {
  DCHECK(!popup_view_);
  popup_view_ = new AutofillPopupViewViews(
      web_contents(), this, element_bounds);
}

void AutofillExternalDelegateViews::ApplyAutofillSuggestions(
    const std::vector<string16>& autofill_values,
    const std::vector<string16>& autofill_labels,
    const std::vector<string16>& autofill_icons,
    const std::vector<int>& autofill_unique_ids) {
  popup_view_->Show(autofill_values,
                    autofill_labels,
                    autofill_icons,
                    autofill_unique_ids);
}
