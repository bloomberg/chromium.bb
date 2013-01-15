// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/phone_number.h"

#include "base/basictypes.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_country.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/field_types.h"
#include "chrome/browser/autofill/phone_number_i18n.h"

namespace {

const char16 kPhoneNumberSeparators[] = { ' ', '.', '(', ')', '-', 0 };

// The number of digits in a phone number.
const size_t kPhoneNumberLength = 7;

// The number of digits in an area code.
const size_t kPhoneCityCodeLength = 3;

void StripPunctuation(string16* number) {
  RemoveChars(*number, kPhoneNumberSeparators, number);
}

// Returns the region code for this phone number, which is an ISO 3166 2-letter
// country code.  The returned value is based on the |profile|; if the |profile|
// does not have a country code associated with it, falls back to the country
// code corresponding to the |app_locale|.
std::string GetRegion(const AutofillProfile& profile,
                      const std::string& app_locale) {
  std::string country_code = profile.CountryCode();
  if (!country_code.empty())
    return country_code;

  return AutofillCountry::CountryCodeForLocale(app_locale);
}

}  // namespace

PhoneNumber::PhoneNumber(AutofillProfile* profile)
    : profile_(profile) {
}

PhoneNumber::PhoneNumber(const PhoneNumber& number)
    : profile_(NULL) {
  *this = number;
}

PhoneNumber::~PhoneNumber() {}

PhoneNumber& PhoneNumber::operator=(const PhoneNumber& number) {
  if (this == &number)
    return *this;

  number_ = number.number_;
  profile_ = number.profile_;
  cached_parsed_phone_ = number.cached_parsed_phone_;
  return *this;
}

void PhoneNumber::GetSupportedTypes(FieldTypeSet* supported_types) const {
  supported_types->insert(PHONE_HOME_WHOLE_NUMBER);
  supported_types->insert(PHONE_HOME_NUMBER);
  supported_types->insert(PHONE_HOME_CITY_CODE);
  supported_types->insert(PHONE_HOME_CITY_AND_NUMBER);
  supported_types->insert(PHONE_HOME_COUNTRY_CODE);
}

string16 PhoneNumber::GetRawInfo(AutofillFieldType type) const {
  if (type == PHONE_HOME_WHOLE_NUMBER)
    return number_;

  // Only the whole number is available as raw data.  All of the other types are
  // parsed from this raw info, and parsing requires knowledge of the phone
  // number's region, which is only available via GetInfo().
  return string16();
}

void PhoneNumber::SetRawInfo(AutofillFieldType type, const string16& value) {
  if (type != PHONE_HOME_CITY_AND_NUMBER &&
      type != PHONE_HOME_WHOLE_NUMBER) {
    // Only full phone numbers should be set directly.  The remaining field
    // field types are read-only.
    return;
  }

  number_ = value;

  // Invalidate the cached number.
  cached_parsed_phone_ = autofill_i18n::PhoneObject();
}

// Normalize phones if |type| is a whole number:
//   (650)2345678 -> 6502345678
//   1-800-FLOWERS -> 18003569377
// If the phone cannot be normalized, returns the stored value verbatim.
string16 PhoneNumber::GetInfo(AutofillFieldType type,
                              const std::string& app_locale) const {
  UpdateCacheIfNeeded(app_locale);

  // Queries for whole numbers will return the non-normalized number if
  // normalization for the number fails.  All other field types require
  // normalization.
  if (type != PHONE_HOME_WHOLE_NUMBER && !cached_parsed_phone_.IsValidNumber())
    return string16();

  switch (type) {
    case PHONE_HOME_WHOLE_NUMBER:
      return cached_parsed_phone_.GetWholeNumber();

    case PHONE_HOME_NUMBER:
      return cached_parsed_phone_.number();

    case PHONE_HOME_CITY_CODE:
      return cached_parsed_phone_.city_code();

    case PHONE_HOME_COUNTRY_CODE:
      return cached_parsed_phone_.country_code();

    case PHONE_HOME_CITY_AND_NUMBER:
      return
          cached_parsed_phone_.city_code() + cached_parsed_phone_.number();

    default:
      NOTREACHED();
      return string16();
  }
}

bool PhoneNumber::SetInfo(AutofillFieldType type,
                          const string16& value,
                          const std::string& app_locale) {
  SetRawInfo(type, value);

  if (number_.empty())
    return true;

  // Store a formatted (i.e., pretty printed) version of the number.
  UpdateCacheIfNeeded(app_locale);
  number_ = cached_parsed_phone_.GetFormattedNumber();
  return !number_.empty();
}

void PhoneNumber::GetMatchingTypes(const string16& text,
                                   const std::string& app_locale,
                                   FieldTypeSet* matching_types) const {
  string16 stripped_text = text;
  StripPunctuation(&stripped_text);
  FormGroup::GetMatchingTypes(stripped_text, app_locale, matching_types);

  // For US numbers, also compare to the three-digit prefix and the four-digit
  // suffix, since web sites often split numbers into these two fields.
  string16 number = GetInfo(PHONE_HOME_NUMBER, app_locale);
  if (GetRegion(*profile_, app_locale) == "US" &&
      number.size() == (kPrefixLength + kSuffixLength)) {
    string16 prefix = number.substr(kPrefixOffset, kPrefixLength);
    string16 suffix = number.substr(kSuffixOffset, kSuffixLength);
    if (text == prefix || text == suffix)
      matching_types->insert(PHONE_HOME_NUMBER);
  }

  string16 whole_number = GetInfo(PHONE_HOME_WHOLE_NUMBER, app_locale);
  if (!whole_number.empty()) {
    string16 normalized_number =
        autofill_i18n::NormalizePhoneNumber(text,
                                            GetRegion(*profile_, app_locale));
    if (normalized_number == whole_number)
      matching_types->insert(PHONE_HOME_WHOLE_NUMBER);
  }
}

void PhoneNumber::UpdateCacheIfNeeded(const std::string& app_locale) const {
  std::string region = GetRegion(*profile_, app_locale);
  if (!number_.empty() && cached_parsed_phone_.region() != region)
    cached_parsed_phone_ = autofill_i18n::PhoneObject(number_, region);
}

PhoneNumber::PhoneCombineHelper::PhoneCombineHelper() {
}

PhoneNumber::PhoneCombineHelper::~PhoneCombineHelper() {
}

bool PhoneNumber::PhoneCombineHelper::SetInfo(AutofillFieldType field_type,
                                              const string16& value) {
  if (field_type == PHONE_HOME_COUNTRY_CODE) {
    country_ = value;
    return true;
  }

  if (field_type == PHONE_HOME_CITY_CODE) {
    city_ = value;
    return true;
  }

  if (field_type == PHONE_HOME_CITY_AND_NUMBER) {
    phone_ = value;
    return true;
  }

  if (field_type == PHONE_HOME_WHOLE_NUMBER) {
    whole_number_ = value;
    return true;
  }

  if (field_type == PHONE_HOME_NUMBER) {
    phone_.append(value);
    return true;
  }

  return false;
}

bool PhoneNumber::PhoneCombineHelper::ParseNumber(
    const AutofillProfile& profile,
    const std::string& app_locale,
    string16* value) {
  if (IsEmpty())
    return false;

  if (!whole_number_.empty()) {
    *value = whole_number_;
    return true;
  }

  return autofill_i18n::ConstructPhoneNumber(
      country_, city_, phone_, GetRegion(profile, app_locale), value);
}

bool PhoneNumber::PhoneCombineHelper::IsEmpty() const {
  return phone_.empty() && whole_number_.empty();
}
