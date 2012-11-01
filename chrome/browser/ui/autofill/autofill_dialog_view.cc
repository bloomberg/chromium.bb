// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cstddef>  // For NULL.

#include "chrome/browser/ui/autofill/autofill_dialog_view.h"

namespace autofill {

AutofillDialogView::~AutofillDialogView() {}

#if !defined(TOOLKIT_VIEWS)
// TODO(estade): implement the dialog on other platforms. See
// http://crbug.com/157274 http://crbug.com/157275 http://crbug.com/157277
AutofillDialogView* AutofillDialogView::Create(
    AutofillDialogController* controller) {
  return NULL;
}
#endif

}  // namespace autofill
