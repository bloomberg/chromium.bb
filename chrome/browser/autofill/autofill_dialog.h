// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_AUTOFILL_DIALOG_H_
#define CHROME_BROWSER_AUTOFILL_AUTOFILL_DIALOG_H_

#include <vector>

#include "chrome/browser/autofill/autofill_profile.h"

// Shows the AutoFill dialog, which allows the user to edit profile information.
// |profiles| is a vector of autofill profiles that contains the current profile
// information.  The dialog fills out the profile fields using this data.  Any
// changes made to the profile information through the dialog should be
// transferred back into |profiles|.
void ShowAutoFillDialog(const std::vector<AutoFillProfile>& profiles);

#endif  // CHROME_BROWSER_AUTOFILL_AUTOFILL_DIALOG_H_
