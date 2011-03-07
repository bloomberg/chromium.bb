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

const AutofillType::FieldTypeSubGroup kAutoFillPhoneTypes[] = {
  AutofillType::PHONE_NUMBER,
  AutofillType::PHONE_CITY_CODE,
  AutofillType::PHONE_COUNTRY_CODE,
  AutofillType::PHONE_CITY_AND_NUMBER,
  AutofillType::PHONE_WHOLE_NUMBER,
};

const int kAutoFillPhoneLength = arraysize(kAutoFillPhoneTypes);

}  // namespace

PhoneNumber::PhoneNumber() {}

PhoneNumber::~PhoneNumber() {}

void PhoneNumber::GetPossibleFieldTypes(const string16& text,
                                        FieldTypeSet* possible_types) const {
  string16 stripped_text(text);
  StripPunctuation(&stripped_text);
  if (!Validate(stripped_text))
    return;

  if (IsNumber(stripped_text))
    possible_types->insert(GetNumberType());

  if (IsCityCode(stripped_text))
    possible_types->insert(GetCityCodeType());

  if (IsCountryCode(stripped_text))
    possible_types->insert(GetCountryCodeType());

  if (IsCityAndNumber(stripped_text))
    possible_types->insert(GetCityAndNumberType());

  if (IsWholeNumber(stripped_text))
    possible_types->insert(GetWholeNumberType());
}

void PhoneNumber::GetAvailableFieldTypes(FieldTypeSet* available_types) const {
  DCHECK(available_types);

  if (!number().empty())
    available_types->insert(GetNumberType());

  if (!city_code().empty())
    available_types->insert(GetCityCodeType());

  if (!country_code().empty())
    available_types->insert(GetCountryCodeType());

  if (!CityAndNumber().empty())
    available_types->insert(GetCityAndNumberType());

  if (!WholeNumber().empty())
    available_types->insert(GetWholeNumberType());
}

string16 PhoneNumber::GetFieldText(const AutofillType& type) const {
  AutofillFieldType field_type = type.field_type();
  if (field_type == GetNumberType())
    return number();

  if (field_type == GetCityCodeType())
    return city_code();

  if (field_type == GetCountryCodeType())
    return country_code();

  if (field_type == GetCityAndNumberType())
    return CityAndNumber();

  if (field_type == GetWholeNumberType())
    return WholeNumber();

  return string16();
}

void PhoneNumber::FindInfoMatches(const AutofillType& type,
                                  const string16& info,
                                  std::vector<string16>* matched_text) const {
  if (matched_text == NULL) {
    DLOG(ERROR) << "NULL matched vector passed in";
    return;
  }

  string16 number(info);
  StripPunctuation(&number);
  if (!Validate(number))
    return;

  string16 match;
  if (type.field_type() == UNKNOWN_TYPE) {
    for (int i = 0; i < kAutoFillPhoneLength; ++i) {
      if (FindInfoMatchesHelper(kAutoFillPhoneTypes[i], info, &match))
        matched_text->push_back(match);
    }
  } else {
    if (FindInfoMatchesHelper(type.subgroup(), info, &match))
      matched_text->push_back(match);
  }
}

void PhoneNumber::SetInfo(const AutofillType& type, const string16& value) {
  string16 number(value);
  StripPunctuation(&number);
  if (!Validate(number))
    return;

  FieldTypeSubGroup subgroup = type.subgroup();
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

PhoneNumber::PhoneNumber(const PhoneNumber& phone_number)
    : FormGroup(),
      country_code_(phone_number.country_code_),
      city_code_(phone_number.city_code_),
      number_(phone_number.number_),
      extension_(phone_number.extension_) {
}

bool PhoneNumber::FindInfoMatchesHelper(const FieldTypeSubGroup& subgroup,
                                        const string16& info,
                                        string16* match) const {
  if (match == NULL) {
    DLOG(ERROR) << "NULL match string passed in";
    return false;
  }

  match->clear();
  if (subgroup == AutofillType::PHONE_NUMBER &&
      StartsWith(number(), info, true)) {
    *match = number();
  } else if (subgroup == AutofillType::PHONE_CITY_CODE &&
             StartsWith(city_code(), info, true)) {
    *match = city_code();
  } else if (subgroup == AutofillType::PHONE_COUNTRY_CODE &&
             StartsWith(country_code(), info, true)) {
    *match = country_code();
  } else if (subgroup == AutofillType::PHONE_CITY_AND_NUMBER &&
             StartsWith(CityAndNumber(), info, true)) {
    *match = CityAndNumber();
  } else if (subgroup == AutofillType::PHONE_WHOLE_NUMBER &&
             StartsWith(WholeNumber(), info, true)) {
    *match = WholeNumber();
  }

  return !match->empty();
}

bool PhoneNumber::IsNumber(const string16& text) const {
  if (text.length() > number_.length())
    return false;

  return StartsWith(number_, text, false);
}

bool PhoneNumber::IsCityCode(const string16& text) const {
  if (text.length() > city_code_.length())
    return false;

  return StartsWith(city_code_, text, false);
}

bool PhoneNumber::IsCountryCode(const string16& text) const {
  if (text.length() > country_code_.length())
    return false;

  return StartsWith(country_code_, text, false);
}

bool PhoneNumber::IsCityAndNumber(const string16& text) const {
  string16 city_and_number(CityAndNumber());
  if (text.length() > city_and_number.length())
    return false;

  return StartsWith(city_and_number, text, false);
}

bool PhoneNumber::IsWholeNumber(const string16& text) const {
  string16 whole_number(WholeNumber());
  if (text.length() > whole_number.length())
    return false;

  return StartsWith(whole_number, text, false);
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
