// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>  // For NULL.

#include "chrome/browser/ui/autofill/autofill_dialog_view.h"

#include "chrome/browser/ui/autofill/testable_autofill_dialog_view.h"

namespace autofill {

AutofillDialogView::~AutofillDialogView() {}

#if defined(TOOLKIT_GTK)
// TODO(estade): implement the dialog on GTK. See http://crbug.com/157275.
AutofillDialogView* AutofillDialogView::Create(
    AutofillDialogController* controller) {
  return NULL;
}
#endif

TestableAutofillDialogView* AutofillDialogView::GetTestableView() {
  return NULL;
}

TestableAutofillDialogView::~TestableAutofillDialogView() {}

}  // namespace autofill
