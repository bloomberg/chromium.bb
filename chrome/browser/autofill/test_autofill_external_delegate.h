// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_TEST_AUTOFILL_EXTERNAL_DELEGATE_H_
#define CHROME_BROWSER_AUTOFILL_TEST_AUTOFILL_EXTERNAL_DELEGATE_H_

#include "chrome/browser/autofill/autofill_external_delegate.h"

class AutofillManager;

namespace autofill {

// Calls the required functions on the given external delegate to cause the
// delegate to display a popup.
void GenerateTestAutofillPopup(
    AutofillExternalDelegate* autofill_external_delegate);

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_TEST_AUTOFILL_EXTERNAL_DELEGATE_H_
