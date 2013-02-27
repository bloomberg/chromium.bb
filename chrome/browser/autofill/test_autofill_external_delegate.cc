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
  gfx::RectF bounds(100.f, 100.f);
  autofill_external_delegate->OnQuery(query_id, form, field, bounds, false);

  std::vector<string16> autofill_item;
  autofill_item.push_back(string16());
  std::vector<int> autofill_id;
  autofill_id.push_back(0);
  autofill_external_delegate->OnSuggestionsReturned(
      query_id, autofill_item, autofill_item, autofill_item, autofill_id);
}

}  // namespace autofill
