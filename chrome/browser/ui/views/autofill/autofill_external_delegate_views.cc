// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_external_delegate_views.h"

#include "chrome/browser/ui/views/autofill/autofill_popup_view_views.h"

AutofillExternalDelegate* AutofillExternalDelegate::Create(
    TabContents* tab_contents,
    AutofillManager* autofill_manager) {
  return new AutofillExternalDelegateViews(tab_contents, autofill_manager);
}

AutofillExternalDelegateViews::AutofillExternalDelegateViews(
    TabContents* tab_contents,
    AutofillManager* autofill_manager)
    : AutofillExternalDelegate(tab_contents, autofill_manager),
      popup_view_(NULL) {
}

AutofillExternalDelegateViews::~AutofillExternalDelegateViews() {
  if (popup_view_)
    popup_view_->Hide();
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

void AutofillExternalDelegateViews::OnQueryPlatformSpecific(
    int query_id,
    const webkit::forms::FormData& form,
    const webkit::forms::FormField& field,
    const gfx::Rect& bounds) {
  CreateViewIfNeeded();
  popup_view_->set_element_bounds(bounds);
}

void AutofillExternalDelegateViews::ApplyAutofillSuggestions(
    const std::vector<string16>& autofill_values,
    const std::vector<string16>& autofill_labels,
    const std::vector<string16>& autofill_icons,
    const std::vector<int>& autofill_unique_ids) {
  CreateViewIfNeeded();

  popup_view_->Show(autofill_values,
                    autofill_labels,
                    autofill_icons,
                    autofill_unique_ids);
}

void AutofillExternalDelegateViews::SetBounds(const gfx::Rect& bounds) {
  popup_view_->SetBoundsRect(bounds);
}

void AutofillExternalDelegateViews::CreateViewIfNeeded() {
  if (!popup_view_)
    popup_view_ = new AutofillPopupViewViews(web_contents(), this);
}
