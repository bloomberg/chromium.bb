// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/autofill/autofill_dialog_controller_mac.h"
#include "chrome/browser/autofill/autofill_dialog.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"

// Mac implementation of |ShowAutoFillDialog| interface defined in
// |chrome/browser/autofill/autofill_dialog.h|.
void ShowAutoFillDialog(gfx::NativeView parent,
                        AutoFillDialogObserver* observer,
                        Profile* profile) {
  [AutoFillDialogController
      showAutoFillDialogWithObserver:observer
                             profile:profile];
}
