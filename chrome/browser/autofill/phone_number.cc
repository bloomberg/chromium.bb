// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/phone_number.h"

#include "base/basictypes.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
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

const AutofillType::FieldTypeSubGroup kAutofillPhoneTypes[] = {
  AutofillType::PHONE_NUMBER,
  AutofillType::PHONE_CITY_CODE,
  AutofillType::PHONE_COUNTRY_CODE,
  AutofillType::PHONE_CITY_AND_NUMBER,
  AutofillType::PHONE_WHOLE_NUMBER,
};

const int kAutofillPhoneLength = arraysize(kAutofillPhoneTypes);

}  // namespace

PhoneNumber::PhoneNumber()
    : phone_group_(AutofillType::NO_GROUP) {
}

PhoneNumber::PhoneNumber(AutofillType::FieldTypeGroup phone_group)
    : phone_group_(phone_group) {
}

PhoneNumber::PhoneNumber(const PhoneNumber& number) : FormGroup() {
  *this = number;
}

PhoneNumber::~PhoneNumber() {}

PhoneNumber& PhoneNumber::operator=(const PhoneNumber& number) {
  if (this == &number)
    return *this;
  number_ = number.number_;
  phone_group_ = number.phone_group_;
  cached_parsed_phone_ = number.cached_parsed_phone_;
  return *this;
}

void PhoneNumber::GetMatchingTypes(const string16& text,
                                   FieldTypeSet* matching_types) const {
  string16 stripped_text(text);
  StripPunctuation(&stripped_text);

  if (!cached_parsed_phone_.IsValidNumber())
    return;

  if (IsNumber(stripped_text, cached_parsed_phone_.GetNumber()))
    matching_types->insert(GetNumberType());

  if (stripped_text == cached_parsed_phone_.GetCityCode())
    matching_types->insert(GetCityCodeType());

  if (stripped_text == cached_parsed_phone_.GetCountryCode())
    matching_types->insert(GetCountryCodeType());

  string16 city_and_local(cached_parsed_phone_.GetCityCode());
  city_and_local.append(cached_parsed_phone_.GetNumber());
  if (stripped_text == city_and_local)
    matching_types->insert(GetCityAndNumberType());

  // Whole number is compared to unfiltered text - it would be parsed for phone
  // comparision (e.g. 1-800-FLOWERS and 18003569377 are the same)
  if (IsWholeNumber(text))
    matching_types->insert(GetWholeNumberType());
}

void PhoneNumber::GetNonEmptyTypes(FieldTypeSet* non_empty_types) const {
  DCHECK(non_empty_types);

  if (number_.empty())
    return;

  non_empty_types->insert(GetWholeNumberType());

  if (!cached_parsed_phone_.IsValidNumber())
    return;

  non_empty_types->insert(GetNumberType());

  if (!cached_parsed_phone_.GetCityCode().empty()) {
    non_empty_types->insert(GetCityCodeType());
    non_empty_types->insert(GetCityAndNumberType());
  }

  if (!cached_parsed_phone_.GetCountryCode().empty())
    non_empty_types->insert(GetCountryCodeType());
}

string16 PhoneNumber::GetInfo(AutofillFieldType type) const {
  if (type == GetWholeNumberType())
    return number_;

  if (!cached_parsed_phone_.IsValidNumber())
    return string16();

  if (type == GetNumberType())
    return cached_parsed_phone_.GetNumber();

  if (type == GetCityCodeType())
    return cached_parsed_phone_.GetCityCode();

  if (type == GetCountryCodeType())
    return cached_parsed_phone_.GetCountryCode();

  string16 city_and_local(cached_parsed_phone_.GetCityCode());
  city_and_local.append(cached_parsed_phone_.GetNumber());
  if (type == GetCityAndNumberType())
    return city_and_local;

  return string16();
}

void PhoneNumber::SetInfo(AutofillFieldType type, const string16& value) {
  FieldTypeSubGroup subgroup = AutofillType(type).subgroup();
  FieldTypeGroup group = AutofillType(type).group();
  if (phone_group_ == AutofillType::NO_GROUP)
    phone_group_ = group;  // First call on empty phone - set the group.
  if (subgroup == AutofillType::PHONE_NUMBER) {
    // Should not be set directly.
    NOTREACHED();
  } else if (subgroup == AutofillType::PHONE_CITY_CODE) {
    // Should not be set directly.
    NOTREACHED();
  } else if (subgroup == AutofillType::PHONE_COUNTRY_CODE) {
    // Should not be set directly.
    NOTREACHED();
  } else if (subgroup == AutofillType::PHONE_CITY_AND_NUMBER ||
             subgroup == AutofillType::PHONE_WHOLE_NUMBER) {
    number_ = value;
    cached_parsed_phone_ = autofill_i18n::PhoneObject(number_, locale_);
    StripPunctuation(&number_);
  } else {
    NOTREACHED();
  }
}

bool PhoneNumber::NormalizePhone() {
  // Empty number does not need normalization.
  if (number_.empty())
    return true;

  number_ = cached_parsed_phone_.GetWholeNumber();
  return !number_.empty();
}

void PhoneNumber::set_locale(const std::string& locale) {
  locale_ = locale;
  if (!number_.empty() && cached_parsed_phone_.GetLocale() != locale_)
    cached_parsed_phone_ = autofill_i18n::PhoneObject(number_, locale_);
}

AutofillFieldType PhoneNumber::GetNumberType() const {
  if (phone_group_ == AutofillType::PHONE_HOME)
    return PHONE_HOME_NUMBER;
  else if (phone_group_ == AutofillType::PHONE_FAX)
    return PHONE_FAX_NUMBER;
  else
    NOTREACHED();
  return UNKNOWN_TYPE;
}

AutofillFieldType PhoneNumber::GetCityCodeType() const {
  if (phone_group_ == AutofillType::PHONE_HOME)
    return PHONE_HOME_CITY_CODE;
  else if (phone_group_ == AutofillType::PHONE_FAX)
    return PHONE_FAX_CITY_CODE;
  else
    NOTREACHED();
  return UNKNOWN_TYPE;
}

AutofillFieldType PhoneNumber::GetCountryCodeType() const {
  if (phone_group_ == AutofillType::PHONE_HOME)
    return PHONE_HOME_COUNTRY_CODE;
  else if (phone_group_ == AutofillType::PHONE_FAX)
    return PHONE_FAX_COUNTRY_CODE;
  else
    NOTREACHED();
  return UNKNOWN_TYPE;
}

AutofillFieldType PhoneNumber::GetCityAndNumberType() const {
  if (phone_group_ == AutofillType::PHONE_HOME)
    return PHONE_HOME_CITY_AND_NUMBER;
  else if (phone_group_ == AutofillType::PHONE_FAX)
    return PHONE_FAX_CITY_AND_NUMBER;
  else
    NOTREACHED();
  return UNKNOWN_TYPE;
}

AutofillFieldType PhoneNumber::GetWholeNumberType() const {
  if (phone_group_ == AutofillType::PHONE_HOME)
    return PHONE_HOME_WHOLE_NUMBER;
  else if (phone_group_ == AutofillType::PHONE_FAX)
    return PHONE_FAX_WHOLE_NUMBER;
  else
    NOTREACHED();
  return UNKNOWN_TYPE;
}

bool PhoneNumber::PhoneCombineHelper::SetInfo(AutofillFieldType field_type,
                                              const string16& value) {
  PhoneNumber temp(phone_group_);

  if (field_type == temp.GetCountryCodeType()) {
    country_ = value;
    return true;
  } else if (field_type == temp.GetCityCodeType()) {
    city_ = value;
    return true;
  } else if (field_type == temp.GetCityAndNumberType()) {
    phone_ = value;
    return true;
  } else if (field_type == temp.GetNumberType()) {
    phone_.append(value);
    return true;
  } else {
    return false;
  }
}

bool PhoneNumber::PhoneCombineHelper::ParseNumber(const std::string& locale,
                                                  string16* value) {
  DCHECK(value);
  return autofill_i18n::ConstructPhoneNumber(
      country_, city_, phone_,
      locale,
      (country_.empty() ?
          autofill_i18n::NATIONAL : autofill_i18n::INTERNATIONAL),
      value);
}

bool PhoneNumber::IsNumber(const string16& text, const string16& number) const {
  // TODO(georgey): This will need to be updated once we add support for
  // international phone numbers.
  const size_t kPrefixLength = 3;
  const size_t kSuffixLength = 4;

  if (text == number)
    return true;
  if (text.length() == kPrefixLength && StartsWith(number, text, true))
    return true;
  if (text.length() == kSuffixLength && EndsWith(number, text, true))
    return true;

  return false;
}

bool PhoneNumber::IsWholeNumber(const string16& text) const {
  return cached_parsed_phone_.ComparePhones(text) ==
      autofill_i18n::PHONES_EQUAL;
}

// Static.
void PhoneNumber::StripPunctuation(string16* number) {
  RemoveChars(*number, kPhoneNumberSeparators, number);
}
