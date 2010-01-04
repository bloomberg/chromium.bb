// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/address.h"

#include "base/basictypes.h"
#include "base/string_util.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/field_types.h"

static const string16 kAddressSplitChars = ASCIIToUTF16("-,#. ");

static const AutoFillType::FieldTypeSubGroup kAutoFillAddressTypes[] = {
  AutoFillType::ADDRESS_LINE1,
  AutoFillType::ADDRESS_LINE2,
  AutoFillType::ADDRESS_CITY,
  AutoFillType::ADDRESS_STATE,
  AutoFillType::ADDRESS_ZIP,
  AutoFillType::ADDRESS_COUNTRY,
};

static const int kAutoFillAddressLength = arraysize(kAutoFillAddressTypes);

void Address::GetPossibleFieldTypes(const string16& text,
                                    FieldTypeSet* possible_types) const {
  if (IsLine1(text))
    possible_types->insert(GetLine1Type());

  if (IsLine2(text))
    possible_types->insert(GetLine2Type());

  if (IsAptNum(text))
    possible_types->insert(GetAptNumType());

  if (IsCity(text))
    possible_types->insert(GetCityType());

  if (IsState(text))
    possible_types->insert(GetStateType());

  if (IsZipCode(text))
    possible_types->insert(GetZipCodeType());

  if (IsCountry(text))
    possible_types->insert(GetCountryType());
}

void Address::FindInfoMatches(const AutoFillType& type,
                              const string16& info,
                              std::vector<string16>* matched_text) const {
  if (matched_text == NULL) {
    DLOG(ERROR) << "NULL matched vector passed in";
    return;
  }

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
  AutoFillFieldType field_type = type.field_type();
  if (field_type == GetLine1Type())
    return line1();

  if (field_type == GetLine2Type())
    return line2();

  if (field_type == GetAptNumType())
    return apt_num();

  if (field_type == GetCityType())
    return city();

  if (field_type == GetStateType())
    return state();

  if (field_type ==  GetZipCodeType())
    return zip_code();

  if (field_type == GetCountryType())
    return country();

  return EmptyString16();
}

void Address::SetInfo(const AutoFillType& type, const string16& value) {
  FieldTypeSubGroup subgroup = type.subgroup();
  if (subgroup == AutoFillType::ADDRESS_LINE1)
    set_line1(value);
  else if (subgroup == AutoFillType::ADDRESS_LINE2)
    set_line2(value);
  else if (subgroup == AutoFillType::ADDRESS_APPT_NUM)
    set_apt_num(value);
  else if (subgroup == AutoFillType::ADDRESS_CITY)
    set_city(value);
  else if (subgroup == AutoFillType::ADDRESS_STATE)
    set_state(value);
  else if (subgroup == AutoFillType::ADDRESS_COUNTRY)
    set_country(value);
  else if (subgroup == AutoFillType::ADDRESS_ZIP)
    set_zip_code(value);
  else
    NOTREACHED();
}

void Address::Clear() {
  line1_tokens_.clear();
  line1_.clear();
  line2_tokens_.clear();
  line2_.clear();
  apt_num_.clear();
  city_.clear();
  state_.clear();
  country_.clear();
  zip_code_.clear();
}

void Address::Clone(const Address& address) {
  set_line1(address.line1());
  set_line2(address.line2());
  set_apt_num(address.apt_num());
  set_city(address.city());
  set_state(address.state());
  set_country(address.country());
  set_zip_code(address.zip_code());
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

Address::Address(const Address& address)
    : line1_tokens_(address.line1_tokens_),
      line2_tokens_(address.line2_tokens_),
      line1_(address.line1_),
      line2_(address.line2_),
      apt_num_(address.apt_num_),
      city_(address.city_),
      state_(address.state_),
      country_(address.country_),
      zip_code_(address.zip_code_) {
}

bool Address::IsLine1(const string16& text) const {
  return IsLineMatch(text, line1_tokens_);
}

bool Address::IsLine2(const string16& text) const {
  return IsLineMatch(text, line2_tokens_);
}

bool Address::IsAptNum(const string16& text) const {
  return (StringToLowerASCII(apt_num_) == text);
}

bool Address::IsCity(const string16& text) const {
  return (StringToLowerASCII(city_) == text);
}

bool Address::IsState(const string16& text) const {
  return (StringToLowerASCII(state_) == text);
}

bool Address::IsCountry(const string16& text) const {
  return (StringToLowerASCII(country_) == text);
}

bool Address::IsZipCode(const string16& text) const {
  return zip_code_ == text;
}

bool Address::FindInfoMatchesHelper(const FieldTypeSubGroup& subgroup,
                                    const string16& info,
                                    string16* match) const {
  if (match == NULL) {
    DLOG(ERROR) << "NULL match string passed in";
    return false;
  }

  match->clear();
  if (subgroup == AutoFillType::ADDRESS_LINE1 &&
      StartsWith(line1(), info, false)) {
    *match = line1();
  } else if (subgroup == AutoFillType::ADDRESS_LINE2 &&
             StartsWith(line2(), info, false)) {
    *match = line2();
  } else if (subgroup == AutoFillType::ADDRESS_APPT_NUM &&
             StartsWith(apt_num(), info, true)) {
    *match = apt_num();
  } else if (subgroup == AutoFillType::ADDRESS_CITY &&
             StartsWith(city(), info, false)) {
    *match = city();
  } else if (subgroup == AutoFillType::ADDRESS_STATE &&
             StartsWith(state(), info, false)) {
    *match = state();
  } else if (subgroup == AutoFillType::ADDRESS_COUNTRY &&
             StartsWith(country(), info, false)) {
    *match = country();
  } else if (subgroup == AutoFillType::ADDRESS_ZIP &&
             StartsWith(zip_code(), info, true)) {
    *match = zip_code();
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
    if (word == *iter)
      return true;
  }

  return false;
}
