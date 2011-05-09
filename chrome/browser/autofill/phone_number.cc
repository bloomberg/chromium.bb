// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/phone_number.h"

#include "base/basictypes.h"
#include "base/string_util.h"
#include "chrome/browser/autofill/autofill_profile.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/field_types.h"

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

PhoneNumber::PhoneNumber() {}

PhoneNumber::PhoneNumber(const PhoneNumber& number) : FormGroup() {
  *this = number;
}

PhoneNumber::~PhoneNumber() {}

PhoneNumber& PhoneNumber::operator=(const PhoneNumber& number) {
  if (this == &number)
    return *this;
  country_code_ = number.country_code_;
  city_code_ = number.city_code_;
  number_ = number.number_;
  extension_ = number.extension_;
  return *this;
}

void PhoneNumber::GetMatchingTypes(const string16& text,
                                   FieldTypeSet* matching_types) const {
  string16 stripped_text(text);
  StripPunctuation(&stripped_text);
  if (!Validate(stripped_text))
    return;

  if (IsNumber(stripped_text))
    matching_types->insert(GetNumberType());

  if (IsCityCode(stripped_text))
    matching_types->insert(GetCityCodeType());

  if (IsCountryCode(stripped_text))
    matching_types->insert(GetCountryCodeType());

  if (IsCityAndNumber(stripped_text))
    matching_types->insert(GetCityAndNumberType());

  if (IsWholeNumber(stripped_text))
    matching_types->insert(GetWholeNumberType());
}

void PhoneNumber::GetNonEmptyTypes(FieldTypeSet* non_empty_typess) const {
  DCHECK(non_empty_typess);

  if (!number().empty())
    non_empty_typess->insert(GetNumberType());

  if (!city_code().empty())
    non_empty_typess->insert(GetCityCodeType());

  if (!country_code().empty())
    non_empty_typess->insert(GetCountryCodeType());

  if (!CityAndNumber().empty())
    non_empty_typess->insert(GetCityAndNumberType());

  if (!WholeNumber().empty())
    non_empty_typess->insert(GetWholeNumberType());
}

string16 PhoneNumber::GetInfo(AutofillFieldType type) const {
  if (type == GetNumberType())
    return number();

  if (type == GetCityCodeType())
    return city_code();

  if (type == GetCountryCodeType())
    return country_code();

  if (type == GetCityAndNumberType())
    return CityAndNumber();

  if (type == GetWholeNumberType())
    return WholeNumber();

  return string16();
}

void PhoneNumber::SetInfo(AutofillFieldType type, const string16& value) {
  string16 number(value);
  StripPunctuation(&number);
  if (!Validate(number))
    return;

  FieldTypeSubGroup subgroup = AutofillType(type).subgroup();
  if (subgroup == AutofillType::PHONE_NUMBER)
    set_number(number);
  else if (subgroup == AutofillType::PHONE_CITY_CODE)
    set_city_code(number);
  else if (subgroup == AutofillType::PHONE_COUNTRY_CODE)
    set_country_code(number);
  else if (subgroup == AutofillType::PHONE_CITY_AND_NUMBER ||
           subgroup == AutofillType::PHONE_WHOLE_NUMBER)
    set_whole_number(number);
  else
    NOTREACHED();
}

// Static.
bool PhoneNumber::ParsePhoneNumber(const string16& value,
                                   string16* number,
                                   string16* city_code,
                                   string16* country_code) {
  DCHECK(number);
  DCHECK(city_code);
  DCHECK(country_code);

  // Make a working copy of value.
  string16 working = value;

  *number = string16();
  *city_code = string16();
  *country_code = string16();

  // First remove any punctuation.
  StripPunctuation(&working);

  if (working.size() < kPhoneNumberLength)
    return false;

  // Treat the last 7 digits as the number.
  *number = working.substr(working.size() - kPhoneNumberLength,
                           kPhoneNumberLength);
  working.resize(working.size() - kPhoneNumberLength);
  if (working.size() < kPhoneCityCodeLength)
    return true;

  // Treat the next three digits as the city code.
  *city_code = working.substr(working.size() - kPhoneCityCodeLength,
                              kPhoneCityCodeLength);
  working.resize(working.size() - kPhoneCityCodeLength);
  if (working.empty())
    return true;

  // Treat any remaining digits as the country code.
  *country_code = working;
  return true;
}

string16 PhoneNumber::WholeNumber() const {
  string16 whole_number;
  if (!country_code_.empty())
    whole_number.append(country_code_);

  if (!city_code_.empty())
    whole_number.append(city_code_);

  if (!number_.empty())
    whole_number.append(number_);

  return whole_number;
}

void PhoneNumber::set_number(const string16& number) {
  string16 digits(number);
  StripPunctuation(&digits);
  number_ = digits;
}

void PhoneNumber::set_whole_number(const string16& whole_number) {
  string16 number, city_code, country_code;
  ParsePhoneNumber(whole_number, &number, &city_code, &country_code);
  set_number(number);
  set_city_code(city_code);
  set_country_code(country_code);
}

bool PhoneNumber::IsNumber(const string16& text) const {
  // TODO(isherman): This will need to be updated once we add support for
  // international phone numbers.
  const size_t kPrefixLength = 3;
  const size_t kSuffixLength = 4;

  if (text == number_)
    return true;
  if (text.length() == kPrefixLength && StartsWith(number_, text, true))
    return true;
  if (text.length() == kSuffixLength && EndsWith(number_, text, true))
    return true;

  return false;
}

bool PhoneNumber::IsCityCode(const string16& text) const {
  return text == city_code_;
}

bool PhoneNumber::IsCountryCode(const string16& text) const {
  return text == country_code_;
}

bool PhoneNumber::IsCityAndNumber(const string16& text) const {
  return text == CityAndNumber();
}

bool PhoneNumber::IsWholeNumber(const string16& text) const {
  return text == WholeNumber();
}

bool PhoneNumber::Validate(const string16& number) const {
  for (size_t i = 0; i < number.length(); ++i) {
    if (!IsAsciiDigit(number[i]))
      return false;
  }

  return true;
}

// Static.
void PhoneNumber::StripPunctuation(string16* number) {
  RemoveChars(*number, kPhoneNumberSeparators, number);
}
