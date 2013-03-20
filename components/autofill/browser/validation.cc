// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/browser/validation.h"

#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "components/autofill/browser/autofill_regexes.h"
#include "components/autofill/browser/credit_card.h"

namespace autofill {

bool IsValidCreditCardExpirationDate(const string16& year,
                                     const string16& month,
                                     const base::Time& now) {
  string16 year_cleaned, month_cleaned;
  TrimWhitespace(year, TRIM_ALL, &year_cleaned);
  TrimWhitespace(month, TRIM_ALL, &month_cleaned);
  if (year_cleaned.length() != 4)
    return false;

  base::Time::Exploded now_exploded;
  now.LocalExplode(&now_exploded);
  size_t current_year = size_t(now_exploded.year);
  size_t current_month = size_t(now_exploded.month);

  size_t cc_year;
  if (!base::StringToSizeT(year_cleaned, &cc_year))
    return false;
  if (cc_year < current_year)
    return false;

  size_t cc_month;
  if (!base::StringToSizeT(month_cleaned, &cc_month))
    return false;
  if (cc_year == current_year && cc_month < current_month)
    return false;

  return true;
}

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

bool IsValidCreditCardSecurityCode(const string16& code,
                                   const string16& number) {
  CreditCard card;
  card.SetRawInfo(CREDIT_CARD_NUMBER, number);
  size_t required_length = 3;
  if (card.type() == kAmericanExpressCard)
    required_length = 4;

  return code.length() == required_length;
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
