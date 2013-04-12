// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/browser/contact_info.h"

#include <stddef.h>
#include <ostream>
#include <string>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "components/autofill/browser/autofill_type.h"
#include "components/autofill/browser/field_types.h"

namespace autofill {

static const AutofillFieldType kAutofillNameInfoTypes[] = {
  NAME_FIRST,
  NAME_MIDDLE,
  NAME_LAST
};

static const size_t kAutofillNameInfoLength =
    arraysize(kAutofillNameInfoTypes);

NameInfo::NameInfo() {}

NameInfo::NameInfo(const NameInfo& info) : FormGroup() {
  *this = info;
}

NameInfo::~NameInfo() {}

NameInfo& NameInfo::operator=(const NameInfo& info) {
  if (this == &info)
    return *this;

  first_ = info.first_;
  middle_ = info.middle_;
  last_ = info.last_;
  return *this;
}

void NameInfo::GetSupportedTypes(FieldTypeSet* supported_types) const {
  supported_types->insert(NAME_FIRST);
  supported_types->insert(NAME_MIDDLE);
  supported_types->insert(NAME_LAST);
  supported_types->insert(NAME_MIDDLE_INITIAL);
  supported_types->insert(NAME_FULL);
}

base::string16 NameInfo::GetRawInfo(AutofillFieldType type) const {
  if (type == NAME_FIRST)
    return first();

  if (type == NAME_MIDDLE)
    return middle();

  if (type == NAME_LAST)
    return last();

  if (type == NAME_MIDDLE_INITIAL)
    return MiddleInitial();

  if (type == NAME_FULL)
    return FullName();

  return base::string16();
}

void NameInfo::SetRawInfo(AutofillFieldType type, const base::string16& value) {
  DCHECK_EQ(AutofillType::NAME, AutofillType(type).group());
  if (type == NAME_FIRST)
    first_ = value;
  else if (type == NAME_MIDDLE || type == NAME_MIDDLE_INITIAL)
    middle_ = value;
  else if (type == NAME_LAST)
    last_ = value;
  else if (type == NAME_FULL)
    SetFullName(value);
  else
    NOTREACHED();
}

base::string16 NameInfo::FullName() const {
  std::vector<base::string16> full_name;
  if (!first_.empty())
    full_name.push_back(first_);

  if (!middle_.empty())
    full_name.push_back(middle_);

  if (!last_.empty())
    full_name.push_back(last_);

  return JoinString(full_name, ' ');
}

base::string16 NameInfo::MiddleInitial() const {
  if (middle_.empty())
    return base::string16();

  base::string16 middle_name(middle());
  base::string16 initial;
  initial.push_back(middle_name[0]);
  return initial;
}

void NameInfo::SetFullName(const base::string16& full) {
  // Clear the names.
  first_ = base::string16();
  middle_ = base::string16();
  last_ = base::string16();

  std::vector<base::string16> full_name_tokens;
  Tokenize(full, ASCIIToUTF16(" "), &full_name_tokens);

  // There are four possibilities: empty; first name; first and last names;
  // first, middle (possibly multiple strings) and then the last name.
  if (full_name_tokens.size() > 0) {
    first_ = full_name_tokens[0];
    if (full_name_tokens.size() > 1) {
      last_ = full_name_tokens.back();
      if (full_name_tokens.size() > 2) {
        full_name_tokens.erase(full_name_tokens.begin());
        full_name_tokens.pop_back();
        middle_ = JoinString(full_name_tokens, ' ');
      }
    }
  }
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

void EmailInfo::GetSupportedTypes(FieldTypeSet* supported_types) const {
  supported_types->insert(EMAIL_ADDRESS);
}

base::string16 EmailInfo::GetRawInfo(AutofillFieldType type) const {
  if (type == EMAIL_ADDRESS)
    return email_;

  return base::string16();
}

void EmailInfo::SetRawInfo(AutofillFieldType type,
                           const base::string16& value) {
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

void CompanyInfo::GetSupportedTypes(FieldTypeSet* supported_types) const {
  supported_types->insert(COMPANY_NAME);
}

base::string16 CompanyInfo::GetRawInfo(AutofillFieldType type) const {
  if (type == COMPANY_NAME)
    return company_name_;

  return base::string16();
}

void CompanyInfo::SetRawInfo(AutofillFieldType type,
                             const base::string16& value) {
  DCHECK_EQ(COMPANY_NAME, type);
  company_name_ = value;
}

}  // namespace autofill
