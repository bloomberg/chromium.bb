// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_popup_view.h"

namespace autofill {

void AutofillPopupView::Hide() {
  hide_called_ = true;
}

AutofillPopupView::AutofillPopupView() : hide_called_(false) {}

AutofillPopupView::~AutofillPopupView() {
  CHECK(hide_called_);
}

}  // namespace autofill
