// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/phone_number_i18n.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_country.h"
#include "third_party/libphonenumber/cpp/src/phonenumberutil.h"

using i18n::phonenumbers::PhoneNumber;
using i18n::phonenumbers::PhoneNumberUtil;

namespace {

std::string SanitizeLocaleCode(const std::string& locale_code) {
  if (locale_code.length() == 2)
    return locale_code;

  return AutofillCountry::CountryCodeForLocale(
      AutofillCountry::ApplicationLocale());
}

i18n::phonenumbers::PhoneNumberUtil::PhoneNumberFormat UtilsTypeToPhoneLibType(
    autofill_i18n::FullPhoneFormat phone_format) {
  switch (phone_format) {
    case autofill_i18n::E164:
      return i18n::phonenumbers::PhoneNumberUtil::E164;
    case autofill_i18n::INTERNATIONAL:
      return i18n::phonenumbers::PhoneNumberUtil::INTERNATIONAL;
    case autofill_i18n::NATIONAL:
      return i18n::phonenumbers::PhoneNumberUtil::NATIONAL;
    case autofill_i18n::RFC3966:
      return i18n::phonenumbers::PhoneNumberUtil::RFC3966;
    default:
      NOTREACHED();
  }
  return i18n::phonenumbers::PhoneNumberUtil::NATIONAL;
}

// Parses the number stored in |value| as it should be interpreted in the given
// |locale|, and stores the results into the remaining arguments.  The |locale|
// should be sanitized prior to calling this function.
bool ParsePhoneNumberInternal(const string16& value,
                              const std::string& locale,
                              string16* country_code,
                              string16* city_code,
                              string16* number,
                              PhoneNumber* i18n_number) {
  DCHECK(number);
  DCHECK(city_code);
  DCHECK(country_code);
  DCHECK(i18n_number);

  number->clear();
  city_code->clear();
  country_code->clear();

  std::string number_text(UTF16ToUTF8(value));

  // Parse phone number based on the locale.
  PhoneNumberUtil* phone_util = PhoneNumberUtil::GetInstance();

  // The |locale| should already be sanitized.
  DCHECK_EQ(2U, locale.size());
  if (phone_util->Parse(number_text, locale.c_str(), i18n_number) !=
      i18n::phonenumbers::PhoneNumberUtil::NO_PARSING_ERROR) {
    return false;
  }

  i18n::phonenumbers::PhoneNumberUtil::ValidationResult validation =
      phone_util->IsPossibleNumberWithReason(*i18n_number);
  if (validation != i18n::phonenumbers::PhoneNumberUtil::IS_POSSIBLE)
    return false;

  // This verifies that number has a valid area code (that in some cases could
  // be empty) for parsed country code. Also verifies that this is a valid
  // number (in US 1234567 is not valid, because numbers do not start with 1).
  if (!phone_util->IsValidNumber(*i18n_number))
    return false;

  std::string national_significant_number;
  phone_util->GetNationalSignificantNumber(*i18n_number,
                                           &national_significant_number);

  std::string area_code;
  std::string subscriber_number;

  int area_length = phone_util->GetLengthOfGeographicalAreaCode(*i18n_number);
  int destination_length =
      phone_util->GetLengthOfNationalDestinationCode(*i18n_number);
  // Some phones have a destination code in lieu of area code: mobile operators
  // in Europe, toll and toll-free numbers in USA, etc. From our point of view
  // these two types of codes are the same.
  if (destination_length > area_length)
    area_length = destination_length;
  if (area_length > 0) {
    area_code = national_significant_number.substr(0, area_length);
    subscriber_number = national_significant_number.substr(area_length);
  } else {
    subscriber_number = national_significant_number;
  }
  *number = UTF8ToUTF16(subscriber_number);
  *city_code = UTF8ToUTF16(area_code);
  *country_code = string16();

  PhoneNumberUtil::NormalizeDigitsOnly(&number_text);
  string16 normalized_number(UTF8ToUTF16(number_text));
  // Check if parsed number has country code and it was not inferred from the
  // locale.
  if (i18n_number->has_country_code()) {
    *country_code = UTF8ToUTF16(
         base::StringPrintf("%d", i18n_number->country_code()));
    if (normalized_number.length() <= national_significant_number.length() &&
        (normalized_number.length() < country_code->length() ||
        normalized_number.compare(0, country_code->length(), *country_code))) {
      country_code->clear();
    }
  }

  return true;
}

}  // namespace

namespace autofill_i18n {

string16 NormalizePhoneNumber(const string16& value,
                              std::string const& locale) {
  string16 number;
  string16 city_code;
  string16 country_code;
  string16 result;
  // Full number - parse it, split it and re-combine into canonical form.
  if (!ParsePhoneNumber(value, locale, &country_code, &city_code, &number))
    return string16();  // Parsing failed - do not store phone.
  if (!autofill_i18n::ConstructPhoneNumber(
          country_code, city_code, number,
          locale,
          (country_code.empty() ?
              autofill_i18n::NATIONAL : autofill_i18n::INTERNATIONAL),
          &result)) {
    // Reconstruction failed - do not store phone.
    return string16();
  }
  std::string result_utf8(UTF16ToUTF8(result));
  PhoneNumberUtil::NormalizeDigitsOnly(&result_utf8);
  return UTF8ToUTF16(result_utf8);
}

bool ParsePhoneNumber(const string16& value,
                      const std::string& locale,
                      string16* country_code,
                      string16* city_code,
                      string16* number) {
  PhoneNumber i18n_number;
  return ParsePhoneNumberInternal(value, SanitizeLocaleCode(locale),
                                  country_code, city_code, number,
                                  &i18n_number);
}

bool ConstructPhoneNumber(const string16& country_code,
                          const string16& city_code,
                          const string16& number,
                          const std::string& locale,
                          FullPhoneFormat phone_format,
                          string16* whole_number) {
  DCHECK(whole_number);

  whole_number->clear();

  std::string normalized_number(UTF16ToUTF8(city_code));
  normalized_number.append(UTF16ToUTF8(number));

  PhoneNumberUtil::NormalizeDigitsOnly(&normalized_number);

  int64 number_int = 0;
  if (!base::StringToInt64(normalized_number, &number_int) || !number_int)
    return false;

  PhoneNumber i18n_number;
  i18n_number.set_national_number(static_cast<uint64>(number_int));

  PhoneNumberUtil* phone_util = PhoneNumberUtil::GetInstance();

  int country_int = phone_util->GetCountryCodeForRegion(
      SanitizeLocaleCode(locale));
  if (!country_code.empty() && !base::StringToInt(country_code, &country_int))
    return false;
  if (country_int)
    i18n_number.set_country_code(country_int);

  i18n::phonenumbers::PhoneNumberUtil::ValidationResult validation =
      phone_util->IsPossibleNumberWithReason(i18n_number);
  if (validation != i18n::phonenumbers::PhoneNumberUtil::IS_POSSIBLE)
    return false;

  // This verifies that number has a valid area code (that in some cases could
  // be empty) for parsed country code. Also verifies that this is a valid
  // number (in US 1234567 is not valid, because numbers do not start with 1).
  if (!phone_util->IsValidNumber(i18n_number))
    return false;

  std::string formatted_number;

  phone_util->Format(i18n_number, UtilsTypeToPhoneLibType(phone_format),
                     &formatted_number);
  *whole_number = UTF8ToUTF16(formatted_number);
  return true;
}

bool PhoneNumbersMatch(const string16& number_a,
                       const string16& number_b,
                       const std::string& country_code) {
  // Sanitize the provided |country_code| before trying to use it for parsing.
  const std::string locale = SanitizeLocaleCode(country_code);

  PhoneNumberUtil* phone_util = PhoneNumberUtil::GetInstance();

  // Parse phone numbers based on the locale
  PhoneNumber i18n_number1;
  if (phone_util->Parse(UTF16ToUTF8(number_a), locale.c_str(), &i18n_number1) !=
      i18n::phonenumbers::PhoneNumberUtil::NO_PARSING_ERROR) {
    return false;
  }

  PhoneNumber i18n_number2;
  if (phone_util->Parse(UTF16ToUTF8(number_b), locale.c_str(), &i18n_number2) !=
      i18n::phonenumbers::PhoneNumberUtil::NO_PARSING_ERROR) {
    return false;
  }

  switch (phone_util->IsNumberMatch(i18n_number1, i18n_number2)) {
    case i18n::phonenumbers::PhoneNumberUtil::INVALID_NUMBER:
    case i18n::phonenumbers::PhoneNumberUtil::NO_MATCH:
      return false;
    case i18n::phonenumbers::PhoneNumberUtil::SHORT_NSN_MATCH:
      return false;
    case i18n::phonenumbers::PhoneNumberUtil::NSN_MATCH:
    case i18n::phonenumbers::PhoneNumberUtil::EXACT_MATCH:
      return true;
    default:
      NOTREACHED();
  }

  return false;
}

PhoneObject::PhoneObject(const string16& number, const std::string& locale)
    : locale_(SanitizeLocaleCode(locale)),
      i18n_number_(NULL) {
  // TODO(isherman): Autofill profiles should always have a |locale| set, but in
  // some cases it should be marked as implicit.  Otherwise, phone numbers
  // might behave differently when they are synced across computers:
  // [ http://crbug.com/100845 ].  Once the bug is fixed, add a DCHECK here to
  // verify.

  scoped_ptr<PhoneNumber> i18n_number(new PhoneNumber);
  if (ParsePhoneNumberInternal(number, locale_, &country_code_, &city_code_,
                               &number_, i18n_number.get())) {
    // Phone successfully parsed - set |i18n_number_| object, |whole_number_|
    // will be set on the first call to GetWholeNumber().
    i18n_number_.reset(i18n_number.release());
  } else {
    // Parsing failed. Store passed phone "as is" into |whole_number_|.
    whole_number_ = number;
  }
}

PhoneObject::PhoneObject(const PhoneObject& other) : i18n_number_(NULL) {
  *this = other;
}

PhoneObject::PhoneObject() : i18n_number_(NULL) {
}

PhoneObject::~PhoneObject() {
}

string16 PhoneObject::GetWholeNumber() const {
  if (i18n_number_.get() && whole_number_.empty()) {
    i18n::phonenumbers::PhoneNumberUtil::PhoneNumberFormat format =
        i18n::phonenumbers::PhoneNumberUtil::INTERNATIONAL;
    if (country_code_.empty())
      format = i18n::phonenumbers::PhoneNumberUtil::NATIONAL;

    std::string formatted_number;
    PhoneNumberUtil* phone_util = PhoneNumberUtil::GetInstance();
    phone_util->Format(*i18n_number_, format, &formatted_number);
    whole_number_ = UTF8ToUTF16(formatted_number);
  }
  return whole_number_;
}

PhoneObject& PhoneObject::operator=(const PhoneObject& other) {
  if (this == &other)
    return *this;

  country_code_ = other.country_code_;
  city_code_ = other.city_code_;
  number_ = other.number_;
  locale_ = other.locale_;
  if (other.i18n_number_.get())
    i18n_number_.reset(new PhoneNumber(*other.i18n_number_));

  return *this;
}

}  // namespace autofill_i18n

