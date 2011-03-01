// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/credit_card.h"

#include <string>

#include "base/basictypes.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "base/string_number_conversions.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/field_types.h"
#include "chrome/common/guid.h"
#include "grit/generated_resources.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebRegularExpression.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebString.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

const char* kCreditCardObfuscationString = "************";

const AutofillFieldType kAutoFillCreditCardTypes[] = {
  CREDIT_CARD_NAME,
  CREDIT_CARD_NUMBER,
  CREDIT_CARD_TYPE,
  CREDIT_CARD_EXP_MONTH,
  CREDIT_CARD_EXP_4_DIGIT_YEAR,
};

const int kAutoFillCreditCardLength = arraysize(kAutoFillCreditCardTypes);

std::string GetCreditCardType(const string16& number) {
  // Credit card number specifications taken from:
  // http://en.wikipedia.org/wiki/Credit_card_numbers and
  // http://www.beachnet.com/~hstiles/cardtype.html
  // Card Type              Prefix(es)                      Length
  // ---------------------------------------------------------------
  // Visa                   4                               13,16
  // American Express       34,37                           15
  // Diners Club            300-305,2014,2149,36,           14,15
  // Discover Card          6011,65                         16
  // JCB                    3                               16
  // JCB                    2131,1800                       15
  // MasterCard             51-55                           16
  // Solo (debit card)      6334,6767                       16,18,19

  // We need at least 4 digits to work with.
  if (number.length() < 4)
    return kGenericCard;

  int first_four_digits = 0;
  if (!base::StringToInt(number.substr(0, 4), &first_four_digits))
    return kGenericCard;

  int first_three_digits = first_four_digits / 10;
  int first_two_digits = first_three_digits / 10;
  int first_digit = first_two_digits / 10;

  switch (number.length()) {
    case 13:
      if (first_digit == 4)
        return kVisaCard;

      break;
    case 14:
      if (first_three_digits >= 300 && first_three_digits <=305)
        return kDinersCard;

      if (first_digit == 36)
        return kDinersCard;

      break;
    case 15:
      if (first_two_digits == 34 || first_two_digits == 37)
        return kAmericanExpressCard;

      if (first_four_digits == 2131 || first_four_digits == 1800)
        return kJCBCard;

      if (first_four_digits == 2014 || first_four_digits == 2149)
        return kDinersCard;

      break;
    case 16:
      if (first_four_digits == 6011 || first_two_digits == 65)
        return kDiscoverCard;

      if (first_four_digits == 6334 || first_four_digits == 6767)
        return kSoloCard;

      if (first_two_digits >= 51 && first_two_digits <= 55)
        return kMasterCard;

      if (first_digit == 3)
        return kJCBCard;

      if (first_digit == 4)
        return kVisaCard;

      break;
    case 18:
    case 19:
      if (first_four_digits == 6334 || first_four_digits == 6767)
        return kSoloCard;

      break;
  }

  return kGenericCard;
}

// Return a version of |number| that has had any non-digit values removed.
const string16 RemoveNonAsciiDigits(const string16& number) {
  string16 stripped;
  for (size_t i = 0; i < number.size(); ++i) {
    if (IsAsciiDigit(number[i]))
      stripped.append(1, number[i]);
  }
  return stripped;
}

}  // namespace

CreditCard::CreditCard(const std::string& guid)
    : expiration_month_(0),
      expiration_year_(0),
      guid_(guid) {
}

CreditCard::CreditCard()
    : expiration_month_(0),
      expiration_year_(0),
      guid_(guid::GenerateGUID()) {
}

CreditCard::CreditCard(const CreditCard& credit_card) : FormGroup() {
  operator=(credit_card);
}

CreditCard::~CreditCard() {}

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
}

void CreditCard::GetAvailableFieldTypes(FieldTypeSet* available_types) const {
  DCHECK(available_types);

  if (!name_on_card().empty())
    available_types->insert(CREDIT_CARD_NAME);

  if (!ExpirationMonthAsString().empty())
    available_types->insert(CREDIT_CARD_EXP_MONTH);

  if (!Expiration2DigitYearAsString().empty())
    available_types->insert(CREDIT_CARD_EXP_2_DIGIT_YEAR);

  if (!Expiration4DigitYearAsString().empty())
    available_types->insert(CREDIT_CARD_EXP_4_DIGIT_YEAR);

  if (!number().empty())
    available_types->insert(CREDIT_CARD_NUMBER);
}

void CreditCard::FindInfoMatches(const AutoFillType& type,
                                 const string16& info,
                                 std::vector<string16>* matched_text) const {
  DCHECK(matched_text);

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
      NOTREACHED();
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
      // We don't handle this case.
      return string16();

    case CREDIT_CARD_NUMBER:
      return number();

    case CREDIT_CARD_VERIFICATION_CODE:
      NOTREACHED();
      return string16();

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
      NOTREACHED();
      return string16();

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

    case CREDIT_CARD_EXP_2_DIGIT_YEAR:
      // This is a read-only attribute.
      break;

    case CREDIT_CARD_EXP_4_DIGIT_YEAR:
      SetExpirationYearFromString(value);
      break;

    case CREDIT_CARD_TYPE:
      // We determine the type based on the number.
      break;

    case CREDIT_CARD_NUMBER: {
      if (StartsWith(value, ASCIIToUTF16(kCreditCardObfuscationString), true)) {
        // this is an obfuscated string. Do not change the real value.
        break;
      }
      set_number(value);
      set_type(ASCIIToUTF16(GetCreditCardType(number())));
      // Update last four digits as well.
      if (value.length() > 4)
        set_last_four_digits(value.substr(value.length() - 4));
      else
        set_last_four_digits(string16());
    }  break;

    case CREDIT_CARD_VERIFICATION_CODE:
      NOTREACHED();
      break;

    default:
      DLOG(ERROR) << "Attempting to set unknown info-type "
                  << type.field_type();
      break;
  }
}

const string16 CreditCard::Label() const {
  return label_;
}

void CreditCard::SetInfoForMonthInputType(const string16& value) {
  // Check if |text| is "yyyy-mm" format first, and check normal month format.
  WebKit::WebRegularExpression re(WebKit::WebString("^[0-9]{4}\\-[0-9]{1,2}$"),
                                  WebKit::WebTextCaseInsensitive);
  bool match = re.match(WebKit::WebString(StringToLowerASCII(value))) != -1;
  if (match) {
    std::vector<string16> year_month;
    base::SplitString(value, L'-', &year_month);
    DCHECK_EQ((int)year_month.size(), 2);
    int num = 0;
    bool converted = false;
    converted = base::StringToInt(year_month[0], &num);
    DCHECK(converted);
    set_expiration_year(num);
    converted = base::StringToInt(year_month[1], &num);
    DCHECK(converted);
    set_expiration_month(num);
  }
}

string16 CreditCard::ObfuscatedNumber() const {
  if (number().empty())
    return string16();  // No CC number, means empty preview.
  string16 result(ASCIIToUTF16(kCreditCardObfuscationString));
  result.append(last_four_digits());

  return result;
}

string16 CreditCard::PreviewSummary() const {
  string16 preview;
  if (number().empty())
    return preview;  // No CC number, means empty preview.
  string16 obfuscated_cc_number = ObfuscatedNumber();
  if (!expiration_month() || !expiration_year())
    return obfuscated_cc_number;  // No expiration date set.
  // TODO(georgey): Internationalize date.
  string16 formatted_date(ExpirationMonthAsString());
  formatted_date.append(ASCIIToUTF16("/"));
  formatted_date.append(Expiration4DigitYearAsString());

  preview = l10n_util::GetStringFUTF16(
      IDS_CREDIT_CARD_NUMBER_PREVIEW_FORMAT,
      obfuscated_cc_number,
      formatted_date);
  return preview;
}

string16 CreditCard::LastFourDigits() const {
  static const size_t kNumLastDigits = 4;

  if (number().size() < kNumLastDigits)
    return string16();

  return number().substr(number().size() - kNumLastDigits, kNumLastDigits);
}

void CreditCard::operator=(const CreditCard& credit_card) {
  if (this == &credit_card)
    return;

  number_ = credit_card.number_;
  name_on_card_ = credit_card.name_on_card_;
  type_ = credit_card.type_;
  last_four_digits_ = credit_card.last_four_digits_;
  expiration_month_ = credit_card.expiration_month_;
  expiration_year_ = credit_card.expiration_year_;
  label_ = credit_card.label_;
  guid_ = credit_card.guid_;
}

int CreditCard::Compare(const CreditCard& credit_card) const {
  // The following CreditCard field types are the only types we store in the
  // WebDB so far, so we're only concerned with matching these types in the
  // credit card.
  const AutofillFieldType types[] = { CREDIT_CARD_NAME,
                                      CREDIT_CARD_NUMBER,
                                      CREDIT_CARD_EXP_MONTH,
                                      CREDIT_CARD_EXP_4_DIGIT_YEAR };
  for (size_t index = 0; index < arraysize(types); ++index) {
    int comparison = GetFieldText(AutoFillType(types[index])).compare(
        credit_card.GetFieldText(AutoFillType(types[index])));
    if (comparison != 0)
      return comparison;
  }

  return 0;
}

bool CreditCard::operator==(const CreditCard& credit_card) const {
  if (label_ != credit_card.label_ || guid_ != credit_card.guid_)
    return false;

  return Compare(credit_card) == 0;
}

bool CreditCard::operator!=(const CreditCard& credit_card) const {
  return !operator==(credit_card);
}

// Use the Luhn formula to validate the number.
// static
bool CreditCard::IsCreditCardNumber(const string16& text) {
  string16 number = RemoveNonAsciiDigits(text);

  if (number.empty())
    return false;

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

bool CreditCard::IsEmpty() const {
  FieldTypeSet types;
  GetAvailableFieldTypes(&types);
  return types.empty();
}

string16 CreditCard::ExpirationMonthAsString() const {
  if (expiration_month_ == 0)
    return string16();

  string16 month = base::IntToString16(expiration_month_);
  if (expiration_month_ >= 10)
    return month;

  string16 zero = ASCIIToUTF16("0");
  zero.append(month);
  return zero;
}

string16 CreditCard::Expiration4DigitYearAsString() const {
  if (expiration_year_ == 0)
    return string16();

  return base::IntToString16(Expiration4DigitYear());
}

string16 CreditCard::Expiration2DigitYearAsString() const {
  if (expiration_year_ == 0)
    return string16();

  return base::IntToString16(Expiration2DigitYear());
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

void CreditCard::set_number(const string16& number) {
  number_ = RemoveNonAsciiDigits(number);
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

bool CreditCard::FindInfoMatchesHelper(const AutofillFieldType& field_type,
                                       const string16& info,
                                       string16* match) const {
  DCHECK(match);

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
      break;
    }

    case CREDIT_CARD_EXP_2_DIGIT_YEAR: {
      string16 exp_year(Expiration2DigitYearAsString());
      if (StartsWith(exp_year, info, true))
        *match = exp_year;
      break;
    }

    case CREDIT_CARD_EXP_4_DIGIT_YEAR: {
      string16 exp_year(Expiration4DigitYearAsString());
      if (StartsWith(exp_year, info, true))
        *match = exp_year;
      break;
    }

    case CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR: {
      string16 exp_date(ExpirationMonthAsString() + ASCIIToUTF16("/") +
                        Expiration2DigitYearAsString());
      if (StartsWith(exp_date, info, true))
        *match = exp_date;
      break;
    }

    case CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR: {
      string16 exp_date(ExpirationMonthAsString() + ASCIIToUTF16("/") +
                        Expiration4DigitYearAsString());
      if (StartsWith(exp_date, info, true))
        *match = exp_date;
      break;
    }

    case CREDIT_CARD_TYPE:
      // We don't handle this case.
      break;

    case CREDIT_CARD_VERIFICATION_CODE:
      NOTREACHED();
      break;

    default:
      break;
  }
  return !match->empty();
}

bool CreditCard::IsNameOnCard(const string16& text) const {
  return StringToLowerASCII(text) == StringToLowerASCII(name_on_card_);
}

bool CreditCard::IsExpirationMonth(const string16& text) const {
  int month;
  if (!base::StringToInt(text, &month))
    return false;

  return expiration_month_ == month;
}

bool CreditCard::Is2DigitExpirationYear(const string16& text) const {
  int year;
  if (!base::StringToInt(text, &year))
    return false;

  return year < 100 && (expiration_year_ % 100) == year;
}

bool CreditCard::Is4DigitExpirationYear(const string16& text) const {
  int year;
  if (!base::StringToInt(text, &year))
    return false;

  return expiration_year_ == year;
}

bool CreditCard::ConvertDate(const string16& date, int* num) const {
  if (!date.empty()) {
    bool converted = base::StringToInt(date, num);
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
std::ostream& operator<<(std::ostream& os, const CreditCard& credit_card) {
  return os
      << UTF16ToUTF8(credit_card.Label())
      << " "
      << credit_card.guid()
      << " "
      << UTF16ToUTF8(credit_card.GetFieldText(AutoFillType(CREDIT_CARD_NAME)))
      << " "
      << UTF16ToUTF8(credit_card.GetFieldText(AutoFillType(CREDIT_CARD_TYPE)))
      << " "
      << UTF16ToUTF8(credit_card.GetFieldText(AutoFillType(CREDIT_CARD_NUMBER)))
      << " "
      << UTF16ToUTF8(credit_card.GetFieldText(
             AutoFillType(CREDIT_CARD_EXP_MONTH)))
      << " "
      << UTF16ToUTF8(credit_card.GetFieldText(
             AutoFillType(CREDIT_CARD_EXP_4_DIGIT_YEAR)));
}

// These values must match the values in WebKitClientImpl in webkit/glue. We
// send these strings to WK, which then asks WebKitClientImpl to load the image
// data.
const char* const kAmericanExpressCard = "americanExpressCC";
const char* const kDinersCard = "dinersCC";
const char* const kDiscoverCard = "discoverCC";
const char* const kGenericCard = "genericCC";
const char* const kJCBCard = "jcbCC";
const char* const kMasterCard = "masterCardCC";
const char* const kSoloCard = "soloCC";
const char* const kVisaCard = "visaCC";
