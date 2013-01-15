// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/phone_number_i18n.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/stringprintf.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_country.h"
#include "third_party/libphonenumber/src/phonenumber_api.h"

using i18n::phonenumbers::PhoneNumber;
using i18n::phonenumbers::PhoneNumberUtil;

namespace {

std::string SanitizeRegion(const std::string& region) {
  if (region.length() == 2)
    return region;

  return AutofillCountry::CountryCodeForLocale(
      AutofillCountry::ApplicationLocale());
}

// Returns true if |phone_number| is valid.
bool IsValidPhoneNumber(const PhoneNumber& phone_number) {
  PhoneNumberUtil* phone_util = PhoneNumberUtil::GetInstance();
  if (!phone_util->IsPossibleNumber(phone_number))
    return false;

  // Verify that the number has a valid area code (that in some cases could be
  // empty) for the parsed country code.  Also verify that this is a valid
  // number (for example, in the US 1234567 is not valid, because numbers do not
  // start with 1).
  if (!phone_util->IsValidNumber(phone_number))
    return false;

  return true;
}

// Formats the given |number| as a human-readable string, and writes the result
// into |formatted_number|.  Also, normalizes the formatted number, and writes
// that result into |normalized_number|.  This function should only be called
// with numbers already known to be valid, i.e. validation should be done prior
// to calling this function.  Note that the |country_code|, which determines
// whether to format in the national or in the international format, is passed
// in explicitly, as |number| might have an implicit country code set, even
// though the original input lacked a country code.
void FormatValidatedNumber(const PhoneNumber& number,
                           const string16& country_code,
                           string16* formatted_number,
                           string16* normalized_number) {
  PhoneNumberUtil::PhoneNumberFormat format =
      country_code.empty() ?
      PhoneNumberUtil::NATIONAL :
      PhoneNumberUtil::INTERNATIONAL;

  PhoneNumberUtil* phone_util = PhoneNumberUtil::GetInstance();
  std::string processed_number;
  phone_util->Format(number, format, &processed_number);

  if (formatted_number)
    *formatted_number = UTF8ToUTF16(processed_number);

  if (normalized_number) {
    phone_util->NormalizeDigitsOnly(&processed_number);
    *normalized_number = UTF8ToUTF16(processed_number);
  }
}

}  // namespace

namespace autofill_i18n {

// Parses the number stored in |value| as it should be interpreted in the given
// |region|, and stores the results into the remaining arguments.  The |region|
// should be sanitized prior to calling this function.
bool ParsePhoneNumber(const string16& value,
                      const std::string& region,
                      string16* country_code,
                      string16* city_code,
                      string16* number,
                      PhoneNumber* i18n_number) {
  country_code->clear();
  city_code->clear();
  number->clear();
  *i18n_number = PhoneNumber();

  std::string number_text(UTF16ToUTF8(value));

  // Parse phone number based on the region.
  PhoneNumberUtil* phone_util = PhoneNumberUtil::GetInstance();

  // The |region| should already be sanitized.
  DCHECK_EQ(2U, region.size());
  if (phone_util->Parse(number_text, region.c_str(), i18n_number) !=
          PhoneNumberUtil::NO_PARSING_ERROR) {
    return false;
  }

  if (!IsValidPhoneNumber(*i18n_number))
    return false;

  std::string national_significant_number;
  phone_util->GetNationalSignificantNumber(*i18n_number,
                                           &national_significant_number);

  int area_length = phone_util->GetLengthOfGeographicalAreaCode(*i18n_number);
  int destination_length =
      phone_util->GetLengthOfNationalDestinationCode(*i18n_number);
  // Some phones have a destination code in lieu of area code: mobile operators
  // in Europe, toll and toll-free numbers in USA, etc. From our point of view
  // these two types of codes are the same.
  if (destination_length > area_length)
    area_length = destination_length;

  std::string area_code;
  std::string subscriber_number;
  if (area_length > 0) {
    area_code = national_significant_number.substr(0, area_length);
    subscriber_number = national_significant_number.substr(area_length);
  } else {
    subscriber_number = national_significant_number;
  }
  *number = UTF8ToUTF16(subscriber_number);
  *city_code = UTF8ToUTF16(area_code);
  *country_code = string16();

  phone_util->NormalizeDigitsOnly(&number_text);
  string16 normalized_number(UTF8ToUTF16(number_text));

  // Check if parsed number has a country code that was not inferred from the
  // region.
  if (i18n_number->has_country_code()) {
    *country_code = UTF8ToUTF16(
        base::StringPrintf("%d", i18n_number->country_code()));
    if (normalized_number.length() <= national_significant_number.length() &&
        !StartsWith(normalized_number, *country_code,
                    true /* case_sensitive */)) {
      country_code->clear();
    }
  }

  return true;
}

string16 NormalizePhoneNumber(const string16& value,
                              std::string const& region) {
  string16 country_code;
  string16 unused_city_code;
  string16 unused_number;
  PhoneNumber phone_number;
  if (!ParsePhoneNumber(value, SanitizeRegion(region), &country_code,
                        &unused_city_code, &unused_number, &phone_number)) {
    return string16();  // Parsing failed - do not store phone.
  }

  string16 normalized_number;
  FormatValidatedNumber(phone_number, country_code, NULL, &normalized_number);
  return normalized_number;
}

bool ConstructPhoneNumber(const string16& country_code,
                          const string16& city_code,
                          const string16& number,
                          const std::string& region,
                          string16* whole_number) {
  whole_number->clear();

  string16 unused_country_code;
  string16 unused_city_code;
  string16 unused_number;
  PhoneNumber phone_number;
  if (!ParsePhoneNumber(country_code + city_code + number,
                        SanitizeRegion(region),
                        &unused_country_code, &unused_city_code, &unused_number,
                        &phone_number)) {
    return false;
  }

  FormatValidatedNumber(phone_number, country_code, whole_number, NULL);
  return true;
}

bool PhoneNumbersMatch(const string16& number_a,
                       const string16& number_b,
                       const std::string& raw_region) {
  // Sanitize the provided |raw_region| before trying to use it for parsing.
  const std::string region = SanitizeRegion(raw_region);

  PhoneNumberUtil* phone_util = PhoneNumberUtil::GetInstance();

  // Parse phone numbers based on the region
  PhoneNumber i18n_number1;
  if (phone_util->Parse(UTF16ToUTF8(number_a), region.c_str(), &i18n_number1) !=
          PhoneNumberUtil::NO_PARSING_ERROR) {
    return false;
  }

  PhoneNumber i18n_number2;
  if (phone_util->Parse(UTF16ToUTF8(number_b), region.c_str(), &i18n_number2) !=
          PhoneNumberUtil::NO_PARSING_ERROR) {
    return false;
  }

  switch (phone_util->IsNumberMatch(i18n_number1, i18n_number2)) {
    case PhoneNumberUtil::INVALID_NUMBER:
    case PhoneNumberUtil::NO_MATCH:
      return false;
    case PhoneNumberUtil::SHORT_NSN_MATCH:
      return false;
    case PhoneNumberUtil::NSN_MATCH:
    case PhoneNumberUtil::EXACT_MATCH:
      return true;
  }

  NOTREACHED();
  return false;
}

PhoneObject::PhoneObject(const string16& number, const std::string& region)
    : region_(SanitizeRegion(region)),
      i18n_number_(NULL) {
  // TODO(isherman): Autofill profiles should always have a |region| set, but in
  // some cases it should be marked as implicit.  Otherwise, phone numbers
  // might behave differently when they are synced across computers:
  // [ http://crbug.com/100845 ].  Once the bug is fixed, add a DCHECK here to
  // verify.

  scoped_ptr<PhoneNumber> i18n_number(new PhoneNumber);
  if (ParsePhoneNumber(number, region_, &country_code_, &city_code_, &number_,
                       i18n_number.get())) {
    // The phone number was successfully parsed, so store the parsed version.
    // The formatted and normalized versions will be set on the first call to
    // the coresponding methods.
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

string16 PhoneObject::GetFormattedNumber() const {
  if (i18n_number_ && formatted_number_.empty()) {
    FormatValidatedNumber(*i18n_number_, country_code_, &formatted_number_,
                          &whole_number_);
  }

  return formatted_number_;
}

string16 PhoneObject::GetWholeNumber() const {
  if (i18n_number_ && whole_number_.empty()) {
    FormatValidatedNumber(*i18n_number_, country_code_, &formatted_number_,
                          &whole_number_);
  }

  return whole_number_;
}

PhoneObject& PhoneObject::operator=(const PhoneObject& other) {
  if (this == &other)
    return *this;

  region_ = other.region_;

  if (other.i18n_number_.get())
    i18n_number_.reset(new PhoneNumber(*other.i18n_number_));
  else
    i18n_number_.reset();

  country_code_ = other.country_code_;
  city_code_ = other.city_code_;
  number_ = other.number_;

  formatted_number_ = other.formatted_number_;
  whole_number_ = other.whole_number_;

  return *this;
}

}  // namespace autofill_i18n
