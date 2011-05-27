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
  i18n::phonenumbers::PhoneNumberUtil::NormalizeDigitsOnly(&result_utf8);
  return UTF8ToUTF16(result_utf8);
}

bool ParsePhoneNumber(const string16& value,
                      const std::string& locale,
                      string16* country_code,
                      string16* city_code,
                      string16* number) {
  DCHECK(number);
  DCHECK(city_code);
  DCHECK(country_code);

  number->clear();
  city_code->clear();
  country_code->clear();

  std::string number_text(UTF16ToUTF8(value));

  // Parse phone number based on the locale.
  i18n::phonenumbers::PhoneNumber i18n_number;
  i18n::phonenumbers::PhoneNumberUtil* phone_util =
      i18n::phonenumbers::PhoneNumberUtil::GetInstance();
  DCHECK(phone_util);

  if (phone_util->Parse(number_text, SanitizeLocaleCode(locale).c_str(),
                        &i18n_number) !=
      i18n::phonenumbers::PhoneNumberUtil::NO_PARSING_ERROR) {
    return false;
  }

  i18n::phonenumbers::PhoneNumberUtil::ValidationResult validation =
      phone_util->IsPossibleNumberWithReason(i18n_number);
  if (validation != i18n::phonenumbers::PhoneNumberUtil::IS_POSSIBLE)
    return false;

  // This verifies that number has a valid area code (that in some cases could
  // be empty) for parsed country code. Also verifies that this is a valid
  // number (in US 1234567 is not valid, because numbers do not start with 1).
  if (!phone_util->IsValidNumber(i18n_number))
    return false;

  std::string national_significant_number;
  phone_util->GetNationalSignificantNumber(i18n_number,
                                           &national_significant_number);

  std::string area_code;
  std::string subscriber_number;

  int area_length = phone_util->GetLengthOfGeographicalAreaCode(i18n_number);
  int destination_length =
      phone_util->GetLengthOfNationalDestinationCode(i18n_number);
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

  i18n::phonenumbers::PhoneNumberUtil::NormalizeDigitsOnly(&number_text);
  string16 normalized_number(UTF8ToUTF16(number_text));
  // Check if parsed number has country code and it was not inferred from the
  // locale.
  if (i18n_number.has_country_code()) {
    *country_code = UTF8ToUTF16(base::StringPrintf("%d",
                                                   i18n_number.country_code()));
    if (normalized_number.length() <= national_significant_number.length() &&
        (normalized_number.length() < country_code->length() ||
        normalized_number.compare(0, country_code->length(), *country_code))) {
      country_code->clear();
    }
  }

  return true;
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

  i18n::phonenumbers::PhoneNumberUtil::NormalizeDigitsOnly(&normalized_number);

  int64 number_int = 0;
  if (!base::StringToInt64(normalized_number, &number_int) || !number_int)
    return false;

  i18n::phonenumbers::PhoneNumber i18n_number;
  i18n_number.set_national_number(static_cast<uint64>(number_int));

  i18n::phonenumbers::PhoneNumberUtil* phone_util =
      i18n::phonenumbers::PhoneNumberUtil::GetInstance();
  DCHECK(phone_util);

  int country_int = phone_util->GetCountryCodeForRegion(
      std::string(SanitizeLocaleCode(locale).c_str()));
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

string16 FormatPhone(const string16& phone, const std::string& locale,
                     FullPhoneFormat phone_format) {
  std::string number_text(UTF16ToUTF8(phone));

  i18n::phonenumbers::PhoneNumberUtil::NormalizeDigitsOnly(&number_text);

  // Parse phone number based on the locale
  i18n::phonenumbers::PhoneNumber i18n_number;
  i18n::phonenumbers::PhoneNumberUtil* phone_util =
      i18n::phonenumbers::PhoneNumberUtil::GetInstance();
  DCHECK(phone_util);

  if (phone_util->Parse(number_text, SanitizeLocaleCode(locale).c_str(),
                        &i18n_number) !=
      i18n::phonenumbers::PhoneNumberUtil::NO_PARSING_ERROR) {
    return string16();
  }
  std::string formatted_number;
  phone_util->Format(i18n_number, UtilsTypeToPhoneLibType(phone_format),
                     &formatted_number);
  return UTF8ToUTF16(formatted_number);
}

PhoneMatch ComparePhones(const string16& phone1, const string16& phone2,
                         const std::string& locale) {
  std::string number_text(UTF16ToUTF8(phone1));

  // Parse phone number based on the locale
  i18n::phonenumbers::PhoneNumber i18n_number1;
  i18n::phonenumbers::PhoneNumberUtil* phone_util =
      i18n::phonenumbers::PhoneNumberUtil::GetInstance();
  DCHECK(phone_util);

  if (phone_util->Parse(number_text, SanitizeLocaleCode(locale).c_str(),
                        &i18n_number1) !=
      i18n::phonenumbers::PhoneNumberUtil::NO_PARSING_ERROR) {
    return PHONES_NOT_EQUAL;
  }
  number_text = UTF16ToUTF8(phone2);

  // Parse phone number based on the locale
  i18n::phonenumbers::PhoneNumber i18n_number2;
  if (phone_util->Parse(number_text, SanitizeLocaleCode(locale).c_str(),
                        &i18n_number2) !=
      i18n::phonenumbers::PhoneNumberUtil::NO_PARSING_ERROR) {
    return PHONES_NOT_EQUAL;
  }
  switch (phone_util->IsNumberMatch(i18n_number1, i18n_number2)) {
    case i18n::phonenumbers::PhoneNumberUtil::INVALID_NUMBER:
    case i18n::phonenumbers::PhoneNumberUtil::NO_MATCH:
      return PHONES_NOT_EQUAL;
    case i18n::phonenumbers::PhoneNumberUtil::SHORT_NSN_MATCH:
      return PHONES_SUBMATCH;
    case i18n::phonenumbers::PhoneNumberUtil::NSN_MATCH:
    case i18n::phonenumbers::PhoneNumberUtil::EXACT_MATCH:
      return PHONES_EQUAL;
    default:
      NOTREACHED();
  }
  return PHONES_NOT_EQUAL;
}

bool PhoneNumbersMatch(const string16& number_a,
                       const string16& number_b,
                       const std::string& country_code) {
  return ComparePhones(number_a, number_b, country_code) == PHONES_EQUAL;
}

}  // namespace autofill_i18n

