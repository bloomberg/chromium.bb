// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_DIALOG_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_DIALOG_H_
#pragma once

#include <vector>

#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/credit_card.h"
#include "ui/gfx/native_widget_types.h"

class Profile;

// An interface the AutoFill dialog uses to notify its clients (observers) when
// the user has applied changes to the AutoFill profile data.
class AutoFillDialogObserver {
 public:
  // The user has confirmed changes by clicking "Apply" or "OK".  Any of the
  // parameters may be NULL. A NULL parameter is treated as not changing the
  // existing data. For example, |OnAutoFillDialogApply(new_profiles, NULL)|
  // only sets the profiles and not the credit cards.
  virtual void OnAutoFillDialogApply(
      std::vector<AutofillProfile>* profiles,
      std::vector<CreditCard>* credit_cards) = 0;

 protected:
  virtual ~AutoFillDialogObserver() {}
};

// Shows the AutoFill dialog, which allows the user to edit profile information.
// |profile| is profile from which you can get vectors of autofill profiles that
// contains the current profile information and credit cards.  The dialog fills
// out the profile fields using this data.
//
// |observer| will be notified by OnAutoFillDialogAccept when the user has
// applied changes.  May not be NULL.
//
// The |parent| parameter (currently only used on Windows) specifies the parent
// view in the view hierarchy.  May be NULL on Mac and gtk.
void ShowAutoFillDialog(gfx::NativeView parent,
                        AutoFillDialogObserver* observer,
                        Profile* profile);

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_DIALOG_H_
