// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/android/autofill/autofill_external_delegate.h"

#include "base/string_util.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/ui/android/autofill/autofill_popup_view_android.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

void AutofillExternalDelegate::CreateForWebContentsAndManager(
    content::WebContents* web_contents,
    AutofillManager* autofill_manager) {
  if (FromWebContents(web_contents))
    return;

  web_contents->SetUserData(
      UserDataKey(),
      new AutofillExternalDelegateAndroid(web_contents, autofill_manager));
}

AutofillExternalDelegateAndroid::AutofillExternalDelegateAndroid(
    content::WebContents* web_contents, AutofillManager* manager)
    : AutofillExternalDelegate(web_contents, manager) {
}

AutofillExternalDelegateAndroid::~AutofillExternalDelegateAndroid() {
}

void AutofillExternalDelegateAndroid::ApplyAutofillSuggestions(
    const std::vector<string16>& autofill_values,
    const std::vector<string16>& autofill_labels,
    const std::vector<string16>& autofill_icons,
    const std::vector<int>& autofill_unique_ids) {
  view_->Show(autofill_values,
              autofill_labels,
              autofill_icons,
              autofill_unique_ids);
}

void AutofillExternalDelegateAndroid::HideAutofillPopupInternal() {
  if (!view_.get())
    return;

  view_->Hide();
  view_.reset();
}

void AutofillExternalDelegateAndroid::CreatePopupForElement(
    const gfx::Rect& element_bounds) {
  DCHECK(!view_.get());

  content::WebContents* contents = web_contents();
  view_.reset(new AutofillPopupViewAndroid(contents, this, element_bounds));
}
