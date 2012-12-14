// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/browser/ui/autofill/autofill_popup_view.h"

#if defined(OS_MACOSX)
AutofillPopupView* AutofillPopupView::Create(
    AutofillPopupController* controller) {
  NOTIMPLEMENTED();
  return NULL;
}
#endif
