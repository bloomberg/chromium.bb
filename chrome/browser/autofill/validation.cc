// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/validation.h"

#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_regexes.h"
#include "chrome/browser/autofill/credit_card.h"

namespace autofill {

bool IsValidCreditCardNumber(const string16& text) {
  string16 number = CreditCard::StripSeparators(text);

  // Credit card numbers are at most 19 digits in length [1]. 12 digits seems to
  // be a fairly safe lower-bound [2].
  // [1] http://www.merriampark.com/anatomycc.htm
  // [2] http://en.wikipedia.org/wiki/Bank_card_number
  const size_t kMinCreditCardDigits = 12;
  const size_t kMaxCreditCardDigits = 19;
  if (number.size() < kMinCreditCardDigits ||
      number.size() > kMaxCreditCardDigits)
    return false;

  // Use the Luhn formula [3] to validate the number.
  // [3] http://en.wikipedia.org/wiki/Luhn_algorithm
  int sum = 0;
  bool odd = false;
  for (string16::reverse_iterator iter = number.rbegin();
       iter != number.rend();
       ++iter) {
    if (!IsAsciiDigit(*iter))
      return false;

    int digit = *iter - '0';
    if (odd) {
      digit *= 2;
      sum += digit / 10 + digit % 10;
    } else {
      sum += digit;
    }
    odd = !odd;
  }

  return (sum % 10) == 0;
}

bool IsValidCreditCardSecurityCode(const string16& text) {
  if (text.size() < 3U || text.size() > 4U)
    return false;

  for (string16::const_iterator iter = text.begin();
       iter != text.end();
       ++iter) {
    if (!IsAsciiDigit(*iter))
      return false;
  }
  return true;
}

bool IsValidEmailAddress(const string16& text) {
  // E-Mail pattern as defined by the WhatWG. (4.10.7.1.5 E-Mail state)
  const string16 kEmailPattern = ASCIIToUTF16(
      "^[a-zA-Z0-9.!#$%&'*+/=?^_`{|}~-]+@"
      "[a-zA-Z0-9-]+(?:\\.[a-zA-Z0-9-]+)*$");
  return MatchesPattern(text, kEmailPattern);
}

bool IsValidZip(const string16& value) {
  const string16 kZipPattern = ASCIIToUTF16("^\\d{5}(-\\d{4})?$");
  return MatchesPattern(value, kZipPattern);
}

}  // namespace autofill
