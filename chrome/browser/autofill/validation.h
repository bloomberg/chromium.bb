// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_VALIDATION_H_
#define CHROME_BROWSER_AUTOFILL_VALIDATION_H_

#include "base/string16.h"

namespace autofill {

// Returns true if |text| looks like a valid credit card number.
// Uses the Luhn formula to validate the number.
bool IsValidCreditCardNumber(const string16& text);

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_VALIDATION_H_
