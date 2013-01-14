// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/test_autofill_external_delegate.h"

#include "ui/gfx/rect.h"

namespace autofill {

void GenerateTestAutofillPopup(
    AutofillExternalDelegate* autofill_external_delegate) {
  int query_id = 1;
  FormData form;
  FormFieldData field;
  field.is_focusable = true;
  field.should_autocomplete = true;
  gfx::Rect bounds(100, 100);
  autofill_external_delegate->OnQuery(query_id, form, field, bounds, false);

  std::vector<string16> autofill_item;
  autofill_item.push_back(string16());
  std::vector<int> autofill_id;
  autofill_id.push_back(0);
  autofill_external_delegate->OnSuggestionsReturned(
      query_id, autofill_item, autofill_item, autofill_item, autofill_id);
}

TestAutofillExternalDelegate::TestAutofillExternalDelegate(
    content::WebContents* web_contents,
    AutofillManager* autofill_manager)
    : AutofillExternalDelegate(web_contents, autofill_manager) {
  // Initialize Controller.
  const gfx::Rect element_bounds;
  AutofillExternalDelegate::EnsurePopupForElement(element_bounds);
}

TestAutofillExternalDelegate::~TestAutofillExternalDelegate() {}

void TestAutofillExternalDelegate::ApplyAutofillSuggestions(
    const std::vector<string16>& autofill_values,
    const std::vector<string16>& autofill_labels,
    const std::vector<string16>& autofill_icons,
    const std::vector<int>& autofill_unique_ids) {}

void TestAutofillExternalDelegate::HideAutofillPopup() {}

void TestAutofillExternalDelegate::EnsurePopupForElement(
    const gfx::Rect& element_bounds) {}

void TestAutofillExternalDelegate::ControllerDestroyed() {}

}  // namespace autofill
