// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/address.h"

#include "base/basictypes.h"
#include "base/string_util.h"
#include "chrome/browser/autofill/autofill_country.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/field_types.h"

namespace {

const char16 kAddressSplitChars[] = {'-', ',', '#', '.', ' ', 0};

const AutoFillType::FieldTypeSubGroup kAutoFillAddressTypes[] = {
  AutoFillType::ADDRESS_LINE1,
  AutoFillType::ADDRESS_LINE2,
  AutoFillType::ADDRESS_CITY,
  AutoFillType::ADDRESS_STATE,
  AutoFillType::ADDRESS_ZIP,
  AutoFillType::ADDRESS_COUNTRY,
};

const int kAutoFillAddressLength = arraysize(kAutoFillAddressTypes);

}  // namespace

Address::Address() {}

Address::~Address() {}

FormGroup* Address::Clone() const {
  return new Address(*this);
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

void Address::FindInfoMatches(const AutoFillType& type,
                              const string16& info,
                              std::vector<string16>* matched_text) const {
  DCHECK(matched_text);

  string16 match;
  if (type.field_type() == UNKNOWN_TYPE) {
    for (int i = 0; i < kAutoFillAddressLength; ++i) {
      if (FindInfoMatchesHelper(kAutoFillAddressTypes[i], info, &match))
        matched_text->push_back(match);
    }
  } else {
    if (FindInfoMatchesHelper(type.subgroup(), info, &match))
      matched_text->push_back(match);
  }
}

string16 Address::GetFieldText(const AutoFillType& type) const {
  AutofillFieldType field_type = type.field_type();
  if (field_type == ADDRESS_HOME_LINE1)
    return line1_;

  if (field_type == ADDRESS_HOME_LINE2)
    return line2_;

  if (field_type == ADDRESS_HOME_CITY)
    return city_;

  if (field_type == ADDRESS_HOME_STATE)
    return state_;

  if (field_type ==  ADDRESS_HOME_ZIP)
    return zip_code_;

  if (field_type == ADDRESS_HOME_COUNTRY)
    return Country();

  return string16();
}

void Address::SetInfo(const AutoFillType& type, const string16& value) {
  FieldTypeSubGroup subgroup = type.subgroup();
  if (subgroup == AutoFillType::ADDRESS_LINE1)
    set_line1(value);
  else if (subgroup == AutoFillType::ADDRESS_LINE2)
    set_line2(value);
  else if (subgroup == AutoFillType::ADDRESS_CITY)
    city_ = value;
  else if (subgroup == AutoFillType::ADDRESS_STATE)
    state_ = value;
  else if (subgroup == AutoFillType::ADDRESS_COUNTRY)
    SetCountry(value);
  else if (subgroup == AutoFillType::ADDRESS_ZIP)
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

Address::Address(const Address& address)
    : FormGroup(),
      line1_tokens_(address.line1_tokens_),
      line2_tokens_(address.line2_tokens_),
      line1_(address.line1_),
      line2_(address.line2_),
      city_(address.city_),
      state_(address.state_),
      country_code_(address.country_code_),
      zip_code_(address.zip_code_) {
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

bool Address::FindInfoMatchesHelper(const FieldTypeSubGroup& subgroup,
                                    const string16& info,
                                    string16* match) const {
  DCHECK(match);

  match->clear();
  if (subgroup == AutoFillType::ADDRESS_LINE1 &&
      StartsWith(line1_, info, false)) {
    *match = line1_;
  } else if (subgroup == AutoFillType::ADDRESS_LINE2 &&
             StartsWith(line2_, info, false)) {
    *match = line2_;
  } else if (subgroup == AutoFillType::ADDRESS_CITY &&
             StartsWith(city_, info, false)) {
    *match = city_;
  } else if (subgroup == AutoFillType::ADDRESS_STATE &&
             StartsWith(state_, info, false)) {
    *match = state_;
  } else if (subgroup == AutoFillType::ADDRESS_COUNTRY &&
             StartsWith(Country(), info, false)) {
    *match = Country();
  } else if (subgroup == AutoFillType::ADDRESS_ZIP &&
             StartsWith(zip_code_, info, true)) {
    *match = zip_code_;
  }

  return !match->empty();
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
