// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/autofill/autofill_dialog_controller_mac.h"
#include "chrome/browser/autofill/autofill_dialog.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/profile.h"
#include "chrome/common/pref_names.h"

// Mac implementation of |ShowAutoFillDialog| interface defined in
// |chrome/browser/autofill/autofill_dialog.h|.
void ShowAutoFillDialog(AutoFillDialogObserver* observer,
                        const std::vector<AutoFillProfile*>& profiles,
                        const std::vector<CreditCard*>& credit_cards,
                        Profile *profile) {
  // It's possible we haven't shown the InfoBar yet, but if the user is in the
  // AutoFill dialog, she doesn't need to be asked to enable or disable
  // AutoFill.
  profile->GetPrefs()->SetBoolean(prefs::kAutoFillInfoBarShown, true);

  [AutoFillDialogController
      showAutoFillDialogWithObserver:observer
                    autoFillProfiles:profiles
                         creditCards:credit_cards
                             profile:profile];
}

