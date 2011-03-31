// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/address.h"

#include <stddef.h>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "chrome/browser/autofill/autofill_country.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/field_types.h"

namespace {

const char16 kAddressSplitChars[] = {'-', ',', '#', '.', ' ', 0};

const AutofillType::FieldTypeSubGroup kAutofillAddressTypes[] = {
  AutofillType::ADDRESS_LINE1,
  AutofillType::ADDRESS_LINE2,
  AutofillType::ADDRESS_CITY,
  AutofillType::ADDRESS_STATE,
  AutofillType::ADDRESS_ZIP,
  AutofillType::ADDRESS_COUNTRY,
};

const int kAutofillAddressLength = arraysize(kAutofillAddressTypes);

}  // namespace

Address::Address() {}

Address::Address(const Address& address) : FormGroup() {
  *this = address;
}

Address::~Address() {}

Address& Address::operator=(const Address& address) {
  if (this == &address)
    return *this;

  line1_tokens_ = address.line1_tokens_;
  line2_tokens_= address.line2_tokens_;
  line1_ = address.line1_;
  line2_ = address.line2_;
  city_ = address.city_;
  state_ = address.state_;
  country_code_ = address.country_code_;
  zip_code_ = address.zip_code_;
  return *this;
}

void Address::GetPossibleFieldTypes(const string16& text,
                                    FieldTypeSet* possible_types) const {
  DCHECK(possible_types);

  // If the text to match against the field types is empty, then no results will
  // match.
  if (text.empty())
    return;

  if (IsLine1(text))
    possible_types->insert(ADDRESS_HOME_LINE1);

  if (IsLine2(text))
    possible_types->insert(ADDRESS_HOME_LINE2);

  if (IsCity(text))
    possible_types->insert(ADDRESS_HOME_CITY);

  if (IsState(text))
    possible_types->insert(ADDRESS_HOME_STATE);

  if (IsZipCode(text))
    possible_types->insert(ADDRESS_HOME_ZIP);

  if (IsCountry(text))
    possible_types->insert(ADDRESS_HOME_COUNTRY);
}

void Address::GetAvailableFieldTypes(FieldTypeSet* available_types) const {
  DCHECK(available_types);

  if (!line1_.empty())
    available_types->insert(ADDRESS_HOME_LINE1);

  if (!line2_.empty())
    available_types->insert(ADDRESS_HOME_LINE2);

  if (!city_.empty())
    available_types->insert(ADDRESS_HOME_CITY);

  if (!state_.empty())
    available_types->insert(ADDRESS_HOME_STATE);

  if (!zip_code_.empty())
    available_types->insert(ADDRESS_HOME_ZIP);

  if (!country_code_.empty())
    available_types->insert(ADDRESS_HOME_COUNTRY);
}

string16 Address::GetInfo(AutofillFieldType type) const {
  if (type == ADDRESS_HOME_LINE1)
    return line1_;

  if (type == ADDRESS_HOME_LINE2)
    return line2_;

  if (type == ADDRESS_HOME_CITY)
    return city_;

  if (type == ADDRESS_HOME_STATE)
    return state_;

  if (type ==  ADDRESS_HOME_ZIP)
    return zip_code_;

  if (type == ADDRESS_HOME_COUNTRY)
    return Country();

  return string16();
}

void Address::SetInfo(AutofillFieldType type, const string16& value) {
  FieldTypeSubGroup subgroup = AutofillType(type).subgroup();
  if (subgroup == AutofillType::ADDRESS_LINE1)
    set_line1(value);
  else if (subgroup == AutofillType::ADDRESS_LINE2)
    set_line2(value);
  else if (subgroup == AutofillType::ADDRESS_CITY)
    city_ = value;
  else if (subgroup == AutofillType::ADDRESS_STATE)
    state_ = value;
  else if (subgroup == AutofillType::ADDRESS_COUNTRY)
    SetCountry(value);
  else if (subgroup == AutofillType::ADDRESS_ZIP)
    zip_code_ = value;
  else
    NOTREACHED();
}

void Address::Clear() {
  line1_tokens_.clear();
  line1_.clear();
  line2_tokens_.clear();
  line2_.clear();
  city_.clear();
  state_.clear();
  country_code_.clear();
  zip_code_.clear();
}

string16 Address::Country() const {
  if (country_code().empty())
    return string16();

  std::string app_locale = AutofillCountry::ApplicationLocale();
  return AutofillCountry(country_code(), app_locale).name();
}

void Address::set_line1(const string16& line1) {
  line1_ = line1;
  line1_tokens_.clear();
  Tokenize(line1, kAddressSplitChars, &line1_tokens_);
  LineTokens::iterator iter;
  for (iter = line1_tokens_.begin(); iter != line1_tokens_.end(); ++iter)
    *iter = StringToLowerASCII(*iter);
}

void Address::set_line2(const string16& line2) {
  line2_ = line2;
  line2_tokens_.clear();
  Tokenize(line2, kAddressSplitChars, &line2_tokens_);
  LineTokens::iterator iter;
  for (iter = line2_tokens_.begin(); iter != line2_tokens_.end(); ++iter)
    *iter = StringToLowerASCII(*iter);
}

void Address::SetCountry(const string16& country) {
  std::string app_locale = AutofillCountry::ApplicationLocale();
  country_code_ = AutofillCountry::GetCountryCode(country, app_locale);
}

bool Address::IsLine1(const string16& text) const {
  return IsLineMatch(text, line1_tokens_);
}

bool Address::IsLine2(const string16& text) const {
  return IsLineMatch(text, line2_tokens_);
}

bool Address::IsCity(const string16& text) const {
  return (StringToLowerASCII(city_) == StringToLowerASCII(text));
}

bool Address::IsState(const string16& text) const {
  return (StringToLowerASCII(state_) == StringToLowerASCII(text));
}

bool Address::IsCountry(const string16& text) const {
  std::string app_locale = AutofillCountry::ApplicationLocale();
  std::string country_code = AutofillCountry::GetCountryCode(text, app_locale);
  return (!country_code.empty() && country_code_ == country_code);
}

bool Address::IsZipCode(const string16& text) const {
  return zip_code_ == text;
}

bool Address::IsLineMatch(const string16& text,
                          const LineTokens& line_tokens) const {
  size_t line_tokens_size = line_tokens.size();
  if (line_tokens_size == 0)
    return false;

  LineTokens text_tokens;
  Tokenize(text, kAddressSplitChars, &text_tokens);
  size_t text_tokens_size = text_tokens.size();
  if (text_tokens_size == 0)
    return false;

  if (text_tokens_size > line_tokens_size)
    return false;

  // If each of the 'words' contained in the text are also present in the line,
  // then we will consider the text to match the line.
  LineTokens::iterator iter;
  for (iter = text_tokens.begin(); iter != text_tokens.end(); ++iter) {
    if (!IsWordInLine(*iter, line_tokens))
      return false;
  }

  return true;
}

bool Address::IsWordInLine(const string16& word,
                           const LineTokens& line_tokens) const {
  LineTokens::const_iterator iter;
  for (iter = line_tokens.begin(); iter != line_tokens.end(); ++iter) {
    if (StringToLowerASCII(word) == *iter)
      return true;
  }

  return false;
}
