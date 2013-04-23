// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/browser/validation.h"

#include "base/string_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "components/autofill/browser/autofill_regexes.h"
#include "components/autofill/browser/credit_card.h"
#include "components/autofill/browser/state_names.h"

using base::StringPiece16;

namespace {

// The separator characters for SSNs.
const char16 kSSNSeparators[] = {' ', '-', 0};

}  // namespace

namespace autofill {

bool IsValidCreditCardExpirationDate(const base::string16& year,
                                     const base::string16& month,
                                     const base::Time& now) {
  base::string16 year_cleaned, month_cleaned;
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

bool IsValidCreditCardNumber(const base::string16& text) {
  base::string16 number = CreditCard::StripSeparators(text);

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
  for (base::string16::reverse_iterator iter = number.rbegin();
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

bool IsValidCreditCardSecurityCode(const base::string16& text) {
  if (text.size() < 3U || text.size() > 4U)
    return false;

  for (base::string16::const_iterator iter = text.begin();
       iter != text.end();
       ++iter) {
    if (!IsAsciiDigit(*iter))
      return false;
  }
  return true;
}

bool IsValidCreditCardSecurityCode(const base::string16& code,
                                   const base::string16& number) {
  CreditCard card;
  card.SetRawInfo(CREDIT_CARD_NUMBER, number);
  size_t required_length = 3;
  if (card.type() == kAmericanExpressCard)
    required_length = 4;

  return code.length() == required_length;
}

bool IsValidEmailAddress(const base::string16& text) {
  // E-Mail pattern as defined by the WhatWG. (4.10.7.1.5 E-Mail state)
  const base::string16 kEmailPattern = ASCIIToUTF16(
      "^[a-zA-Z0-9.!#$%&'*+/=?^_`{|}~-]+@"
      "[a-zA-Z0-9-]+(?:\\.[a-zA-Z0-9-]+)*$");
  return MatchesPattern(text, kEmailPattern);
}

bool IsValidState(const base::string16& text) {
  return !state_names::GetAbbreviationForName(text).empty() ||
         !state_names::GetNameForAbbreviation(text).empty();
}

bool IsValidZip(const base::string16& text) {
  const base::string16 kZipPattern = ASCIIToUTF16("^\\d{5}(-\\d{4})?$");
  return MatchesPattern(text, kZipPattern);
}

bool IsSSN(const string16& text) {
  string16 number_string;
  RemoveChars(text, kSSNSeparators, &number_string);

  // A SSN is of the form AAA-GG-SSSS (A = area number, G = group number, S =
  // serial number). The validation we do here is simply checking if the area,
  // group, and serial numbers are valid.
  //
  // Historically, the area number was assigned per state, with the group number
  // ascending in an alternating even/odd sequence. With that scheme it was
  // possible to check for validity by referencing a table that had the highest
  // group number assigned for a given area number. (This was something that
  // Chromium never did though, because the "high group" values were constantly
  // changing.)
  //
  // However, starting on 25 June 2011 the SSA began issuing SSNs randomly from
  // all areas and groups. Group numbers and serial numbers of zero remain
  // invalid, and areas 000, 666, and 900-999 remain invalid.
  //
  // References for current practices:
  //   http://www.socialsecurity.gov/employer/randomization.html
  //   http://www.socialsecurity.gov/employer/randomizationfaqs.html
  //
  // References for historic practices:
  //   http://www.socialsecurity.gov/history/ssn/geocard.html
  //   http://www.socialsecurity.gov/employer/stateweb.htm
  //   http://www.socialsecurity.gov/employer/ssnvhighgroup.htm

  if (number_string.length() != 9 || !IsStringASCII(number_string))
    return false;

  int area;
  if (!base::StringToInt(StringPiece16(number_string.begin(),
                                       number_string.begin() + 3),
                         &area)) {
    return false;
  }
  if (area < 1 ||
      area == 666 ||
      area >= 900) {
    return false;
  }

  int group;
  if (!base::StringToInt(StringPiece16(number_string.begin() + 3,
                                       number_string.begin() + 5),
                         &group)
      || group == 0) {
    return false;
  }

  int serial;
  if (!base::StringToInt(StringPiece16(number_string.begin() + 5,
                                       number_string.begin() + 9),
                         &serial)
      || serial == 0) {
    return false;
  }

  return true;
}

}  // namespace autofill
