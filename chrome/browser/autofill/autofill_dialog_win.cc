// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/autofill_dialog.h"
#include "chrome/browser/autofill/autofill_profiles_view_win.h"

void ShowAutoFillDialog(AutoFillDialogObserver* observer,
                        const std::vector<AutoFillProfile*>& profiles,
                        const std::vector<CreditCard*>& credit_cards) {
  AutoFillProfilesView::Show(observer, profiles, credit_cards);
}

