// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_dialog.h"

// TODO(georgey, dhollowa): Remove these as each platform implements this
// function.  The last one to implement the function should remove this file.
#if defined(OS_WIN) || defined(OS_MACOSX)
void ShowAutoFillDialog(AutoFillDialogObserver* observer,
                        const std::vector<AutoFillProfile*>& profiles,
                        const std::vector<CreditCard*>& credit_cards) {
}
#endif  // defined(OS_WIN) || defined(OS_MACOSX)
