// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_BROWSER_VALIDATION_H_
#define COMPONENTS_AUTOFILL_BROWSER_VALIDATION_H_

#include "base/string16.h"

namespace base {
class Time;
}  // namespace base;

namespace autofill {

// Returns true if |year| and |month| describe a date later than |now|.
// |year| must have 4 digits.
bool IsValidCreditCardExpirationDate(const string16& year,
                                     const string16& month,
                                     const base::Time& now);

// Returns true if |text| looks like a valid credit card number.
// Uses the Luhn formula to validate the number.
bool IsValidCreditCardNumber(const string16& text);

// Returns true if |text| looks like a valid credit card security code.
bool IsValidCreditCardSecurityCode(const string16& text);

// Returns true if |code| looks like a valid credit card security code
// for the type of credit card designated by |number|.
bool IsValidCreditCardSecurityCode(const string16& code,
                                   const string16& number);

// Returns true if |text| looks like a valid e-mail address.
bool IsValidEmailAddress(const string16& text);

// Returns true if |text| looks like a valid zip code.
// Valid for US zip codes only.
bool IsValidZip(const string16& text);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_BROWSER_VALIDATION_H_
