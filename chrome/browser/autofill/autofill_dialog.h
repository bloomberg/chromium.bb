// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_DIALOG_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_DIALOG_H_

#include <vector>

#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/credit_card.h"

// An interface the AutoFill dialog uses to notify its clients (observers) when
// the user has applied changes to the AutoFill profile data.
class AutoFillDialogObserver {
 public:
  // The user has confirmed changes by clicking "Apply" or "OK".  Any of the
  // parameters may be NULL.
  virtual void OnAutoFillDialogApply(
      std::vector<AutoFillProfile>* profiles,
      std::vector<CreditCard>* credit_cards) = 0;

 protected:
  virtual ~AutoFillDialogObserver() {}
};

// Shows the AutoFill dialog, which allows the user to edit profile information.
// |profiles| is a vector of autofill profiles that contains the current profile
// information.  The dialog fills out the profile fields using this data.  Any
// changes made to the profile information through the dialog should be
// transferred back into |profiles| and |credit_cards|. |observer| will be
// notified by OnAutoFillDialogAccept when the user has applied changes.
//
// The PersonalDataManager owns the contents of these vectors.  The lifetime of
// the contents is until the PersonalDataManager replaces them with new data
// whenever the web database is updated.
void ShowAutoFillDialog(AutoFillDialogObserver* observer,
                        const std::vector<AutoFillProfile*>& profiles,
                        const std::vector<CreditCard*>& credit_cards);

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_DIALOG_H_
