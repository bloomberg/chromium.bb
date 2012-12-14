// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"

const size_t AutofillPopupView::kBorderThickness = 1;
const size_t AutofillPopupView::kIconPadding = 5;
const size_t AutofillPopupView::kEndPadding = 3;
const size_t AutofillPopupView::kDeleteIconHeight = 16;
const size_t AutofillPopupView::kDeleteIconWidth = 16;
const size_t AutofillPopupView::kAutofillIconHeight = 16;
const size_t AutofillPopupView::kAutofillIconWidth = 25;

#if defined(OS_MACOSX)
AutofillPopupView* AutofillPopupView::Create(
    AutofillPopupController* controller) {
  NOTIMPLEMENTED();
  return NULL;
}
#endif
