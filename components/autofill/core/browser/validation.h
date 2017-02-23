// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_VALIDATION_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_VALIDATION_H_

#include "base/strings/string16.h"
#include "base/strings/string_piece.h"
#include "components/autofill/core/browser/field_types.h"

namespace base {
class Time;
}  // namespace base

namespace autofill {

// Returns true if |year| and |month| describe a date later than |now|.
// |year| must have 4 digits.
bool IsValidCreditCardExpirationDate(int year,
                                     int month,
                                     const base::Time& now);

// Returns true if |text| looks like a valid credit card number.
// Uses the Luhn formula to validate the number.
bool IsValidCreditCardNumber(const base::string16& text);

// Returns true if |code| looks like a valid credit card security code
// for the given credit card type.
bool IsValidCreditCardSecurityCode(const base::string16& code,
                                   const base::StringPiece card_type);

// Returns true if |text| is a supported card type and a valid credit card
// number. |error_message| can't be null and will be filled with the appropriate
// error message.
bool IsValidCreditCardNumberForBasicCardNetworks(
    const base::string16& text,
    const std::set<std::string>& supported_basic_card_networks,
    base::string16* error_message);

// Returns true if |text| looks like a valid e-mail address.
bool IsValidEmailAddress(const base::string16& text);

// Returns true if |text| is a valid US state name or abbreviation.  It is case
// insensitive.  Valid for US states only.
bool IsValidState(const base::string16& text);

// Returns true if |text| looks like a valid zip code.
// Valid for US zip codes only.
bool IsValidZip(const base::string16& text);

// Returns true if |text| looks like an SSN, with or without separators.
bool IsSSN(const base::string16& text);

// Returns whether |value| is valid for the given |type|. If not null,
// |error_message| is populated when the function returns false.
bool IsValidForType(const base::string16& value,
                    ServerFieldType type,
                    base::string16* error_message);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_VALIDATION_H_
