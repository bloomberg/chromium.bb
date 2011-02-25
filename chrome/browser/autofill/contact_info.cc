// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/contact_info.h"

#include "base/basictypes.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/field_types.h"

static const string16 kNameSplitChars = ASCIIToUTF16("-'. ");

static const AutoFillFieldType kAutoFillContactInfoTypes[] = {
  NAME_FIRST,
  NAME_MIDDLE,
  NAME_LAST,
  EMAIL_ADDRESS,
  COMPANY_NAME,
};

static const size_t kAutoFillContactInfoLength =
    arraysize(kAutoFillContactInfoTypes);

ContactInfo::ContactInfo() {}

ContactInfo::~ContactInfo() {}

FormGroup* ContactInfo::Clone() const {
  return new ContactInfo(*this);
}

void ContactInfo::GetPossibleFieldTypes(const string16& text,
                                        FieldTypeSet* possible_types) const {
  DCHECK(possible_types);

  if (IsFirstName(text))
    possible_types->insert(NAME_FIRST);

  if (IsMiddleName(text))
    possible_types->insert(NAME_MIDDLE);

  if (IsLastName(text))
    possible_types->insert(NAME_LAST);

  if (IsMiddleInitial(text))
    possible_types->insert(NAME_MIDDLE_INITIAL);

  if (IsSuffix(text))
    possible_types->insert(NAME_SUFFIX);

  if (IsFullName(text))
    possible_types->insert(NAME_FULL);

  if (email_ == text)
    possible_types->insert(EMAIL_ADDRESS);

  if (company_name_ == text)
    possible_types->insert(COMPANY_NAME);
}

void ContactInfo::GetAvailableFieldTypes(FieldTypeSet* available_types) const {
  DCHECK(available_types);

  if (!first().empty())
    available_types->insert(NAME_FIRST);

  if (!middle().empty())
    available_types->insert(NAME_MIDDLE);

  if (!last().empty())
    available_types->insert(NAME_LAST);

  if (!MiddleInitial().empty())
    available_types->insert(NAME_MIDDLE_INITIAL);

  if (!FullName().empty())
    available_types->insert(NAME_FULL);

  if (!suffix().empty())
    available_types->insert(NAME_SUFFIX);

  if (!email().empty())
    available_types->insert(EMAIL_ADDRESS);

  if (!company_name().empty())
    available_types->insert(COMPANY_NAME);
}

void ContactInfo::FindInfoMatches(const AutoFillType& type,
                                  const string16& info,
                                  std::vector<string16>* matched_text) const {
  DCHECK(matched_text);

  string16 match;
  if (type.field_type() == UNKNOWN_TYPE) {
    for (size_t i = 0; i < kAutoFillContactInfoLength; i++) {
      if (FindInfoMatchesHelper(kAutoFillContactInfoTypes[i], info, &match))
        matched_text->push_back(match);
    }
  } else if (FindInfoMatchesHelper(type.field_type(), info, &match)) {
      matched_text->push_back(match);
  }
}

string16 ContactInfo::GetFieldText(const AutoFillType& type) const {
  AutoFillFieldType field_type = type.field_type();
  if (field_type == NAME_FIRST)
    return first();

  if (field_type == NAME_MIDDLE)
    return middle();

  if (field_type == NAME_LAST)
    return last();

  if (field_type == NAME_MIDDLE_INITIAL)
    return MiddleInitial();

  if (field_type == NAME_FULL)
    return FullName();

  if (field_type == NAME_SUFFIX)
    return suffix();

  if (field_type == EMAIL_ADDRESS)
    return email();

  if (field_type == COMPANY_NAME)
    return company_name();

  return string16();
}

void ContactInfo::SetInfo(const AutoFillType& type, const string16& value) {
  AutoFillFieldType field_type = type.field_type();
  DCHECK_EQ(AutoFillType::CONTACT_INFO, type.group());
  if (field_type == NAME_FIRST)
    SetFirst(value);
  else if (field_type == NAME_MIDDLE || field_type == NAME_MIDDLE_INITIAL)
    SetMiddle(value);
  else if (field_type == NAME_LAST)
    SetLast(value);
  else if (field_type == NAME_SUFFIX)
    set_suffix(value);
  else if (field_type == EMAIL_ADDRESS)
    email_ = value;
  else if (field_type == COMPANY_NAME)
    company_name_ = value;
  else if (field_type == NAME_FULL)
    SetFullName(value);
  else
    NOTREACHED();
}

ContactInfo::ContactInfo(const ContactInfo& contact_info)
    : FormGroup(),
      first_tokens_(contact_info.first_tokens_),
      middle_tokens_(contact_info.middle_tokens_),
      last_tokens_(contact_info.last_tokens_),
      first_(contact_info.first_),
      middle_(contact_info.middle_),
      last_(contact_info.last_),
      suffix_(contact_info.suffix_),
      email_(contact_info.email_),
      company_name_(contact_info.company_name_) {
}

string16 ContactInfo::FullName() const {
  if (first_.empty())
    return string16();

  std::vector<string16> full_name;
  full_name.push_back(first_);

  if (!middle_.empty())
    full_name.push_back(middle_);

  if (!last_.empty())
    full_name.push_back(last_);

  if (!suffix_.empty())
    full_name.push_back(suffix_);

  return JoinString(full_name, ' ');
}

string16 ContactInfo::MiddleInitial() const {
  if (middle_.empty())
    return string16();

  string16 middle_name(middle());
  string16 initial;
  initial.push_back(middle_name[0]);
  return initial;
}

bool ContactInfo::FindInfoMatchesHelper(const AutoFillFieldType& field_type,
                                        const string16& info,
                                        string16* match) const {
  if (match == NULL) {
    DLOG(ERROR) << "NULL match string passed in";
    return false;
  }

  match->clear();
  if (field_type == NAME_FIRST &&
      StartsWith(first(), info, false)) {
    *match = first();
  } else if (field_type == NAME_MIDDLE &&
             StartsWith(middle(), info, false)) {
    *match = middle();
  } else if (field_type == NAME_LAST &&
             StartsWith(last(), info, false)) {
    *match = last();
  } else if (field_type == NAME_SUFFIX &&
             StartsWith(suffix(), info, false)) {
    *match = suffix();
  } else if (field_type == NAME_MIDDLE_INITIAL && IsMiddleInitial(info)) {
    *match = MiddleInitial();
  } else if (field_type == NAME_FULL &&
             StartsWith(FullName(), info, false)) {
    *match = FullName();
  } else if (field_type == EMAIL_ADDRESS &&
             StartsWith(email(), info, false)) {
    *match = email();
  } else if (field_type == COMPANY_NAME &&
             StartsWith(company_name(), info, false)) {
    *match = company_name();
  }

  return !match->empty();
}

// If each of the 'words' contained in the text are also present in the first
// name then we will consider the text to be of type kFirstName. This means
// that people with multiple first names will be able to enter any one of
// their first names and have it correctly recognized.
bool ContactInfo::IsFirstName(const string16& text) const {
  return IsNameMatch(text, first_tokens_);
}

// If each of the 'words' contained in the text are also present in the middle
// name then we will consider the text to be of type kMiddleName.
bool ContactInfo::IsMiddleName(const string16& text) const {
  return IsNameMatch(text, middle_tokens_);
}

// If each of the 'words' contained in the text are also present in the last
// name then we will consider the text to be of type kLastName.
bool ContactInfo::IsLastName(const string16& text) const {
  return IsNameMatch(text, last_tokens_);
}

bool ContactInfo::IsSuffix(const string16& text) const {
  string16 lower_suffix = StringToLowerASCII(suffix_);
  return (lower_suffix == text);
}

bool ContactInfo::IsMiddleInitial(const string16& text) const {
  if (text.length() != 1)
     return false;

  string16 lower_case = StringToLowerASCII(text);
  // If the text entered was a single character and it matches the first letter
  // of any of the given middle names then we consider it to be a middle
  // initial field.
  size_t middle_tokens_size = middle_tokens_.size();
  for (size_t i = 0; i < middle_tokens_size; ++i) {
    if (middle_tokens_[i][0] == lower_case[0])
      return true;
  }

  return false;
}

// A field will be considered to be of type NAME_FULL if:
//    1) it contains at least one word from the first name.
//    2) it contains at least one word from the last name.
//    3) all of the words in the field match a word in either the first,
//       middle, or last name.
bool ContactInfo::IsFullName(const string16& text) const {
  size_t first_tokens_size = first_tokens_.size();
  if (first_tokens_size == 0)
    return false;

  size_t middle_tokens_size = middle_tokens_.size();

  size_t last_tokens_size = last_tokens_.size();
  if (last_tokens_size == 0)
    return false;

  NameTokens text_tokens;
  Tokenize(text, kNameSplitChars, &text_tokens);
  size_t text_tokens_size = text_tokens.size();
  if (text_tokens_size == 0 || text_tokens_size < 2)
    return false;

  size_t name_tokens_size =
      first_tokens_size + middle_tokens_size + last_tokens_size;
  if (text_tokens_size > name_tokens_size)
    return false;

  bool first_name_match = false;
  bool last_name_match = false;
  NameTokens::iterator iter;
  for (iter = text_tokens.begin(); iter != text_tokens.end(); ++iter) {
    bool match = false;
    if (IsWordInName(*iter, first_tokens_)) {
      match = true;
      first_name_match = true;
    }

    if (IsWordInName(*iter, last_tokens_)) {
      match = true;
      last_name_match = true;
    }

    if (IsWordInName(*iter, middle_tokens_))
      match = true;

    if (!match)
      return false;
  }

  return (first_name_match && last_name_match);
}

bool ContactInfo::IsNameMatch(const string16& text,
                              const NameTokens& name_tokens) const {
  size_t name_tokens_size = name_tokens.size();
  if (name_tokens_size == 0)
    return false;

  NameTokens text_tokens;
  Tokenize(text, kNameSplitChars, &text_tokens);
  size_t text_tokens_size = text_tokens.size();
  if (text_tokens_size == 0)
    return false;

  if (text_tokens_size > name_tokens_size)
    return false;

  // If each of the 'words' contained in the text are also present in the name,
  // then we will consider the text to match the name.
  NameTokens::iterator iter;
  for (iter = text_tokens.begin(); iter != text_tokens.end(); ++iter) {
    if (!IsWordInName(*iter, name_tokens))
      return false;
  }

  return true;
}

bool ContactInfo::IsWordInName(const string16& word,
                               const NameTokens& name_tokens) const {
  NameTokens::const_iterator iter;
  for (iter = name_tokens.begin(); iter != name_tokens.end(); ++iter) {
    // |*iter| is already lower-cased.
    if (StringToLowerASCII(word) == *iter)
      return true;
  }

  return false;
}

void ContactInfo::SetFirst(const string16& first) {
  first_ = first;
  first_tokens_.clear();
  Tokenize(first, kNameSplitChars, &first_tokens_);
  NameTokens::iterator iter;
  for (iter = first_tokens_.begin(); iter != first_tokens_.end(); ++iter)
    *iter = StringToLowerASCII(*iter);
}

void ContactInfo::SetMiddle(const string16& middle) {
  middle_ = middle;
  middle_tokens_.clear();
  Tokenize(middle, kNameSplitChars, &middle_tokens_);
  NameTokens::iterator iter;
  for (iter = middle_tokens_.begin(); iter != middle_tokens_.end(); ++iter)
    *iter = StringToLowerASCII(*iter);
}

void ContactInfo::SetLast(const string16& last) {
  last_ = last;
  last_tokens_.clear();
  Tokenize(last, kNameSplitChars, &last_tokens_);
  NameTokens::iterator iter;
  for (iter = last_tokens_.begin(); iter != last_tokens_.end(); ++iter)
    *iter = StringToLowerASCII(*iter);
}

void ContactInfo::SetFullName(const string16& full) {
  NameTokens full_name_tokens;
  Tokenize(full, ASCIIToUTF16(" "), &full_name_tokens);
  // Clear the names.
  SetFirst(string16());
  SetMiddle(string16());
  SetLast(string16());

  // There are four possibilities: empty; first name; first and last names;
  // first, middle (possibly multiple strings) and then the last name.
  if (full_name_tokens.size() > 0) {
    SetFirst(full_name_tokens[0]);
    if (full_name_tokens.size() > 1) {
      SetLast(full_name_tokens.back());
      if (full_name_tokens.size() > 2) {
        full_name_tokens.erase(full_name_tokens.begin());
        full_name_tokens.pop_back();
        SetMiddle(JoinString(full_name_tokens, ' '));
      }
    }
  }
}

