// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/contact_info.h"

#include <stddef.h>
#include <ostream>
#include <string>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_type.h"

namespace autofill {

namespace {

const char* const name_prefixes[] = {
    "1lt", "1st", "2lt", "2nd", "3rd", "admiral", "capt", "captain", "col",
    "cpt", "dr", "gen", "general", "lcdr", "lt", "ltc", "ltg", "ltjg", "maj",
    "major", "mg", "mr", "mrs", "ms", "pastor", "prof", "rep", "reverend",
    "rev", "sen", "st" };

const char* const name_suffixes[] = {
    "b.a", "ba", "d.d.s", "dds", "i", "ii", "iii", "iv", "ix", "jr", "m.a",
    "m.d", "ma", "md", "ms", "ph.d", "phd", "sr", "v", "vi", "vii", "viii",
    "x" };

const char* const family_name_prefixes[] = {
    "d'", "de", "del", "der", "di", "la", "le", "mc", "san", "st", "ter",
    "van", "von" };

// Returns true if |set| contains |element|, modulo a final period.
bool ContainsString(const char* const set[],
                    size_t set_size,
                    const base::string16& element) {
  if (!base::IsStringASCII(element))
    return false;

  base::string16 trimmed_element;
  base::TrimString(element, base::ASCIIToUTF16("."), &trimmed_element);

  for (size_t i = 0; i < set_size; ++i) {
    if (LowerCaseEqualsASCII(trimmed_element, set[i]))
      return true;
  }

  return false;
}

// Removes common name prefixes from |name_tokens|.
void StripPrefixes(std::vector<base::string16>* name_tokens) {
  std::vector<base::string16>::iterator iter = name_tokens->begin();
  while(iter != name_tokens->end()) {
    if (!ContainsString(name_prefixes, arraysize(name_prefixes), *iter))
      break;
    ++iter;
  }

  std::vector<base::string16> copy_vector;
  copy_vector.assign(iter, name_tokens->end());
  *name_tokens = copy_vector;
}

// Removes common name suffixes from |name_tokens|.
void StripSuffixes(std::vector<base::string16>* name_tokens) {
  while(!name_tokens->empty()) {
    if (!ContainsString(name_suffixes, arraysize(name_suffixes),
                        name_tokens->back())) {
      break;
    }
    name_tokens->pop_back();
  }
}

struct NameParts {
  base::string16 given;
  base::string16 middle;
  base::string16 family;
};

// TODO(estade): This does Western name splitting. It should do different
// splitting based on the app locale.
NameParts SplitName(const base::string16& name) {
  std::vector<base::string16> name_tokens;
  Tokenize(name, base::ASCIIToUTF16(" ,"), &name_tokens);

  StripPrefixes(&name_tokens);

  // Don't assume "Ma" is a suffix in John Ma.
  if (name_tokens.size() > 2)
    StripSuffixes(&name_tokens);

  NameParts parts;

  if (name_tokens.empty()) {
    // Bad things have happened; just assume the whole thing is a given name.
    parts.given = name;
    return parts;
  }

  // Only one token, assume given name.
  if (name_tokens.size() == 1) {
    parts.given = name_tokens[0];
    return parts;
  }

  // 2 or more tokens. Grab the family, which is the last word plus any
  // recognizable family prefixes.
  std::vector<base::string16> reverse_family_tokens;
  reverse_family_tokens.push_back(name_tokens.back());
  name_tokens.pop_back();
  while (name_tokens.size() >= 1 &&
         ContainsString(family_name_prefixes,
                        arraysize(family_name_prefixes),
                        name_tokens.back())) {
    reverse_family_tokens.push_back(name_tokens.back());
    name_tokens.pop_back();
  }

  std::vector<base::string16> family_tokens(reverse_family_tokens.rbegin(),
                                            reverse_family_tokens.rend());
  parts.family = JoinString(family_tokens, base::char16(' '));

  // Take the last remaining token as the middle name (if there are at least 2
  // tokens).
  if (name_tokens.size() >= 2) {
    parts.middle = name_tokens.back();
    name_tokens.pop_back();
  }

  // Remainder is given name.
  parts.given = JoinString(name_tokens, base::char16(' '));

  return parts;
}

}  // namespace

NameInfo::NameInfo() {}

NameInfo::NameInfo(const NameInfo& info) : FormGroup() {
  *this = info;
}

NameInfo::~NameInfo() {}

NameInfo& NameInfo::operator=(const NameInfo& info) {
  if (this == &info)
    return *this;

  given_ = info.given_;
  middle_ = info.middle_;
  family_ = info.family_;
  full_ = info.full_;
  return *this;
}

bool NameInfo::ParsedNamesAreEqual(const NameInfo& info) {
  return (base::StringToLowerASCII(given_) ==
              base::StringToLowerASCII(info.given_) &&
          base::StringToLowerASCII(middle_) ==
              base::StringToLowerASCII(info.middle_) &&
          base::StringToLowerASCII(family_) ==
              base::StringToLowerASCII(info.family_));
}

void NameInfo::GetSupportedTypes(ServerFieldTypeSet* supported_types) const {
  supported_types->insert(NAME_FIRST);
  supported_types->insert(NAME_MIDDLE);
  supported_types->insert(NAME_LAST);
  supported_types->insert(NAME_MIDDLE_INITIAL);
  supported_types->insert(NAME_FULL);
}

base::string16 NameInfo::GetRawInfo(ServerFieldType type) const {
  DCHECK_EQ(NAME, AutofillType(type).group());
  switch (type) {
    case NAME_FIRST:
      return given_;

    case NAME_MIDDLE:
      return middle_;

    case NAME_LAST:
      return family_;

    case NAME_MIDDLE_INITIAL:
      return MiddleInitial();

    case NAME_FULL:
      return full_;

    default:
      return base::string16();
  }
}

void NameInfo::SetRawInfo(ServerFieldType type, const base::string16& value) {
  DCHECK_EQ(NAME, AutofillType(type).group());

  switch (type) {
    case NAME_FIRST:
      given_ = value;
      break;

    case NAME_MIDDLE:
    case NAME_MIDDLE_INITIAL:
      middle_ = value;
      break;

    case NAME_LAST:
      family_ = value;
      break;

    case NAME_FULL:
      full_ = value;
      break;

    default:
      NOTREACHED();
  }
}

base::string16 NameInfo::GetInfo(const AutofillType& type,
                                 const std::string& app_locale) const {
  if (type.GetStorableType() == NAME_FULL)
    return FullName();

  return GetRawInfo(type.GetStorableType());
}

bool NameInfo::SetInfo(const AutofillType& type,
                       const base::string16& value,
                       const std::string& app_locale) {
  // Always clear out the full name if we're making a change.
  if (value != GetInfo(type, app_locale))
    full_.clear();

  if (type.GetStorableType() == NAME_FULL) {
    SetFullName(value);
    return true;
  }

  return FormGroup::SetInfo(type, value, app_locale);
}

base::string16 NameInfo::FullName() const {
  if (!full_.empty())
    return full_;

  std::vector<base::string16> full_name;
  if (!given_.empty())
    full_name.push_back(given_);

  if (!middle_.empty())
    full_name.push_back(middle_);

  if (!family_.empty())
    full_name.push_back(family_);

  return JoinString(full_name, ' ');
}

base::string16 NameInfo::MiddleInitial() const {
  if (middle_.empty())
    return base::string16();

  base::string16 middle_name(middle_);
  base::string16 initial;
  initial.push_back(middle_name[0]);
  return initial;
}

void NameInfo::SetFullName(const base::string16& full) {
  full_ = full;

  // If |full| is empty, leave the other name parts alone. This might occur
  // due to a migrated database with an empty |full_name| value.
  if (full.empty())
    return;

  NameParts parts = SplitName(full);
  given_ = parts.given;
  middle_ = parts.middle;
  family_ = parts.family;
}

EmailInfo::EmailInfo() {}

EmailInfo::EmailInfo(const EmailInfo& info) : FormGroup() {
  *this = info;
}

EmailInfo::~EmailInfo() {}

EmailInfo& EmailInfo::operator=(const EmailInfo& info) {
  if (this == &info)
    return *this;

  email_ = info.email_;
  return *this;
}

void EmailInfo::GetSupportedTypes(ServerFieldTypeSet* supported_types) const {
  supported_types->insert(EMAIL_ADDRESS);
}

base::string16 EmailInfo::GetRawInfo(ServerFieldType type) const {
  if (type == EMAIL_ADDRESS)
    return email_;

  return base::string16();
}

void EmailInfo::SetRawInfo(ServerFieldType type, const base::string16& value) {
  DCHECK_EQ(EMAIL_ADDRESS, type);
  email_ = value;
}

CompanyInfo::CompanyInfo() {}

CompanyInfo::CompanyInfo(const CompanyInfo& info) : FormGroup() {
  *this = info;
}

CompanyInfo::~CompanyInfo() {}

CompanyInfo& CompanyInfo::operator=(const CompanyInfo& info) {
  if (this == &info)
    return *this;

  company_name_ = info.company_name_;
  return *this;
}

void CompanyInfo::GetSupportedTypes(ServerFieldTypeSet* supported_types) const {
  supported_types->insert(COMPANY_NAME);
}

base::string16 CompanyInfo::GetRawInfo(ServerFieldType type) const {
  if (type == COMPANY_NAME)
    return company_name_;

  return base::string16();
}

void CompanyInfo::SetRawInfo(ServerFieldType type,
                             const base::string16& value) {
  DCHECK_EQ(COMPANY_NAME, type);
  company_name_ = value;
}

}  // namespace autofill
