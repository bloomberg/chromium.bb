// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/credit_card.h"

#include "base/basictypes.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/field_types.h"

static const string16 kCreditCardSeparators = ASCIIToUTF16(" -");

static const AutoFillFieldType kAutoFillCreditCardTypes[] = {
  CREDIT_CARD_NAME,
  CREDIT_CARD_NUMBER,
  CREDIT_CARD_TYPE,
  CREDIT_CARD_EXP_MONTH,
  CREDIT_CARD_EXP_4_DIGIT_YEAR,
  CREDIT_CARD_VERIFICATION_CODE,
};

static const int kAutoFillCreditCardLength =
    arraysize(kAutoFillCreditCardTypes);

CreditCard::CreditCard(const string16& label, int unique_id)
    : expiration_month_(0),
      expiration_year_(0),
      label_(label),
      unique_id_(unique_id) {
}

CreditCard::CreditCard(const CreditCard& card) {
  operator=(card);
}
FormGroup* CreditCard::Clone() const {
  return new CreditCard(*this);
}

void CreditCard::GetPossibleFieldTypes(const string16& text,
                                       FieldTypeSet* possible_types) const {
  if (IsNameOnCard(text))
    possible_types->insert(CREDIT_CARD_NAME);

  if (IsCreditCardNumber(text))
    possible_types->insert(CREDIT_CARD_NUMBER);

  if (IsExpirationMonth(text))
    possible_types->insert(CREDIT_CARD_EXP_MONTH);

  if (Is2DigitExpirationYear(text))
    possible_types->insert(CREDIT_CARD_EXP_2_DIGIT_YEAR);

  if (Is4DigitExpirationYear(text))
    possible_types->insert(CREDIT_CARD_EXP_4_DIGIT_YEAR);

  if (IsCardType(text))
    possible_types->insert(CREDIT_CARD_TYPE);

  if (IsVerificationCode(text))
    possible_types->insert(CREDIT_CARD_VERIFICATION_CODE);
}

void CreditCard::FindInfoMatches(const AutoFillType& type,
                                 const string16& info,
                                 std::vector<string16>* matched_text) const {
  if (matched_text == NULL) {
    DLOG(ERROR) << "NULL matched vector passed in";
    return;
  }

  string16 match;
  switch (type.field_type()) {
    case CREDIT_CARD_NUMBER: {
      // Because the credit card number is encrypted and we are not able to do
      // comparisons with it we will say that any field that is known to be a
      // credit card number field will match all credit card numbers.
      string16 text = GetPreviewText(AutoFillType(CREDIT_CARD_NUMBER));
      if (!text.empty())
        matched_text->push_back(text);
      break;
    }

    case CREDIT_CARD_VERIFICATION_CODE:
      break;

    case UNKNOWN_TYPE:
      for (int i = 0; i < kAutoFillCreditCardLength; ++i) {
        if (FindInfoMatchesHelper(kAutoFillCreditCardTypes[i], info, &match))
          matched_text->push_back(match);
      }
      break;

    default:
      if (FindInfoMatchesHelper(type.field_type(), info, &match))
        matched_text->push_back(match);
      break;
  }
}

string16 CreditCard::GetFieldText(const AutoFillType& type) const {
  switch (type.field_type()) {
    case CREDIT_CARD_NAME:
      return name_on_card();

    case CREDIT_CARD_EXP_MONTH:
      return ExpirationMonthAsString();

    case CREDIT_CARD_EXP_2_DIGIT_YEAR:
      return Expiration2DigitYearAsString();

    case CREDIT_CARD_EXP_4_DIGIT_YEAR:
      return Expiration4DigitYearAsString();

    case CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR: {
      string16 month = ExpirationMonthAsString();
      string16 year = Expiration2DigitYearAsString();
      if (!month.empty() && !year.empty())
        return month + ASCIIToUTF16("/") + year;
      return string16();
    }

    case CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR: {
      string16 month = ExpirationMonthAsString();
      string16 year = Expiration4DigitYearAsString();
      if (!month.empty() && !year.empty())
        return month + ASCIIToUTF16("/") + year;
      return string16();
    }

    case CREDIT_CARD_TYPE:
      return this->type();

    case CREDIT_CARD_NUMBER:
      return number();

    case CREDIT_CARD_VERIFICATION_CODE:
      return verification_code();

    default:
      // ComputeDataPresentForArray will hit this repeatedly.
      return string16();
  }
}

string16 CreditCard::GetPreviewText(const AutoFillType& type) const {
  switch (type.field_type()) {
    case CREDIT_CARD_NUMBER:
      return last_four_digits();

    case CREDIT_CARD_VERIFICATION_CODE:
      return ASCIIToUTF16("xxx");

    default:
      return GetFieldText(type);
  }
}

void CreditCard::SetInfo(const AutoFillType& type, const string16& value) {
  switch (type.field_type()) {
    case CREDIT_CARD_NAME:
      set_name_on_card(value);
      break;

    case CREDIT_CARD_EXP_MONTH:
      SetExpirationMonthFromString(value);
      break;

    case CREDIT_CARD_EXP_4_DIGIT_YEAR:
      SetExpirationYearFromString(value);
      break;

    case CREDIT_CARD_TYPE:
      set_type(value);
      break;

    case CREDIT_CARD_NUMBER:
      set_number(value);
      break;

    case CREDIT_CARD_VERIFICATION_CODE:
      set_verification_code(value);
      break;

    default:
      DLOG(ERROR) << "Attempting to set unknown info-type "
                  << type.field_type();
      break;
  }
}

string16 CreditCard::ExpirationMonthAsString() const {
  if (expiration_month_ == 0)
    return string16();

  string16 month = IntToString16(expiration_month_);
  if (expiration_month_ >= 10)
    return month;

  string16 zero = ASCIIToUTF16("0");
  zero.append(month);
  return zero;
}

string16 CreditCard::Expiration4DigitYearAsString() const {
  if (expiration_year_ == 0)
    return string16();

  return IntToString16(Expiration4DigitYear());
}

string16 CreditCard::Expiration2DigitYearAsString() const {
  if (expiration_year_ == 0)
    return string16();

  return IntToString16(Expiration2DigitYear());
}

void CreditCard::SetExpirationMonthFromString(const string16& text) {
  int month;
  if (!ConvertDate(text, &month))
    return;

  set_expiration_month(month);
}

void CreditCard::SetExpirationYearFromString(const string16& text) {
  int year;
  if (!ConvertDate(text, &year))
    return;

  set_expiration_year(year);
}

void CreditCard::set_expiration_month(int expiration_month) {
  if (expiration_month < 0 || expiration_month > 12)
    return;

  expiration_month_ = expiration_month;
}

void CreditCard::set_expiration_year(int expiration_year) {
  if (expiration_year != 0 &&
      (expiration_year < 2006 || expiration_year > 10000)) {
    return;
  }

  expiration_year_ = expiration_year;
}


void CreditCard::operator=(const CreditCard& source) {
  number_ = source.number_;
  name_on_card_ = source.name_on_card_;
  type_ = source.type_;
  verification_code_ = source.verification_code_;
  last_four_digits_ = source.last_four_digits_;
  expiration_month_ = source.expiration_month_;
  expiration_year_ = source.expiration_year_;
  label_ = source.label_;
  billing_address_ = source.billing_address_;
  shipping_address_ = source.shipping_address_;
  unique_id_ = source.unique_id_;
}

bool CreditCard::operator==(const CreditCard& creditcard) const {
  // The following CreditCard field types are the only types we store in the
  // WebDB so far, so we're only concerned with matching these types in the
  // profile.
  const AutoFillFieldType types[] = { CREDIT_CARD_NAME,
                                      CREDIT_CARD_TYPE,
                                      CREDIT_CARD_NUMBER,
                                      CREDIT_CARD_VERIFICATION_CODE,
                                      CREDIT_CARD_EXP_MONTH,
                                      CREDIT_CARD_EXP_4_DIGIT_YEAR };

  if (label_ != creditcard.label_ ||
      unique_id_ != creditcard.unique_id_ ||
      billing_address_ != creditcard.billing_address_ ||
      shipping_address_ != creditcard.shipping_address_) {
    return false;
  }

  for (size_t index = 0; index < arraysize(types); ++index) {
    if (GetFieldText(AutoFillType(types[index])) !=
        creditcard.GetFieldText(AutoFillType(types[index])))
      return false;
  }

  return true;
}

bool CreditCard::FindInfoMatchesHelper(const AutoFillFieldType& field_type,
                                       const string16& info,
                                       string16* match) const {
  if (match == NULL) {
    DLOG(ERROR) << "NULL match string passed in";
    return false;
  }

  match->clear();
  switch (field_type) {
    case CREDIT_CARD_NAME: {
      if (StartsWith(name_on_card(), info, false))
        *match = name_on_card();
      break;
    }

    case CREDIT_CARD_EXP_MONTH: {
      string16 exp_month(ExpirationMonthAsString());
      if (StartsWith(exp_month, info, true))
        *match = exp_month;
    }

    case CREDIT_CARD_EXP_2_DIGIT_YEAR: {
      string16 exp_year(Expiration2DigitYearAsString());
      if (StartsWith(exp_year, info, true))
        *match = exp_year;
    }

    case CREDIT_CARD_EXP_4_DIGIT_YEAR: {
      string16 exp_year(Expiration4DigitYearAsString());
      if (StartsWith(exp_year, info, true))
        *match = exp_year;
    }

    case CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR: {
      string16 exp_date(ExpirationMonthAsString() + ASCIIToUTF16("/") +
                        Expiration2DigitYearAsString());
      if (StartsWith(exp_date, info, true))
        *match = exp_date;
    }

    case CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR: {
      string16 exp_date(ExpirationMonthAsString() + ASCIIToUTF16("/") +
                        Expiration4DigitYearAsString());
      if (StartsWith(exp_date, info, true))
        *match = exp_date;
    }

    case CREDIT_CARD_TYPE: {
      string16 card_type(this->type());
      if (StartsWith(card_type, info, true))
        *match = card_type;
    }

    case CREDIT_CARD_VERIFICATION_CODE: {
      if (StartsWith(verification_code(), info, true))
        *match = verification_code();
    }

    default:
      break;
  }
  return !match->empty();
}

bool CreditCard::IsNameOnCard(const string16& text) const {
  return StringToLowerASCII(text) == StringToLowerASCII(name_on_card_);
}

bool CreditCard::IsCreditCardNumber(const string16& text) {
  string16 number;
  TrimString(text, kCreditCardSeparators.c_str(), &number);

  // We use the Luhn formula to validate the number; see
  // http://www.beachnet.com/~hstiles/cardtype.html and
  // http://www.webopedia.com/TERM/L/Luhn_formula.html.
  int sum = 0;
  bool odd = false;
  string16::reverse_iterator iter;
  for (iter = number.rbegin(); iter != number.rend(); ++iter) {
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

bool CreditCard::IsVerificationCode(const string16& text) const {
  return StringToLowerASCII(text) == StringToLowerASCII(verification_code_);
}

bool CreditCard::IsExpirationMonth(const string16& text) const {
  int month;
  if (!StringToInt(text, &month))
    return false;

  return expiration_month_ == month;
}

bool CreditCard::Is2DigitExpirationYear(const string16& text) const {
  int year;
  if (!StringToInt(text, &year))
    return false;

  return year < 100 && (expiration_year_ % 100) == year;
}

bool CreditCard::Is4DigitExpirationYear(const string16& text) const {
  int year;
  if (!StringToInt(text, &year))
    return false;

  return expiration_year_ == year;
}

bool CreditCard::IsCardType(const string16& text) const {
  return StringToLowerASCII(text) == StringToLowerASCII(type_);
}

bool CreditCard::ConvertDate(const string16& date, int* num) const {
  if (!date.empty()) {
    bool converted = StringToInt(date, num);
    DCHECK(converted);
    if (!converted)
      return false;
  } else {
    // Clear the value.
    *num = 0;
  }

  return true;
}

// So we can compare CreditCards with EXPECT_EQ().
std::ostream& operator<<(std::ostream& os, const CreditCard& creditcard) {
  return os
      << UTF16ToUTF8(creditcard.Label())
      << " "
      << creditcard.unique_id()
      << " "
      << UTF16ToUTF8(creditcard.billing_address())
      << " "
      << UTF16ToUTF8(creditcard.shipping_address())
      << " "
      << UTF16ToUTF8(creditcard.GetFieldText(AutoFillType(CREDIT_CARD_NAME)))
      << " "
      << UTF16ToUTF8(creditcard.GetFieldText(AutoFillType(CREDIT_CARD_TYPE)))
      << " "
      << UTF16ToUTF8(creditcard.GetFieldText(AutoFillType(CREDIT_CARD_NUMBER)))
      << " "
      << UTF16ToUTF8(creditcard.GetFieldText(
             AutoFillType(CREDIT_CARD_VERIFICATION_CODE)))
      << " "
      << UTF16ToUTF8(creditcard.GetFieldText(
             AutoFillType(CREDIT_CARD_EXP_MONTH)))
      << " "
      << UTF16ToUTF8(creditcard.GetFieldText(
             AutoFillType(CREDIT_CARD_EXP_4_DIGIT_YEAR)));
}
