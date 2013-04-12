// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/browser/address.h"

#include <stddef.h>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "components/autofill/browser/autofill_country.h"
#include "components/autofill/browser/autofill_field.h"
#include "components/autofill/browser/autofill_type.h"
#include "components/autofill/browser/field_types.h"

namespace {

const char16 kAddressSplitChars[] = {'-', ',', '#', '.', ' ', 0};

}  // namespace

namespace autofill {

Address::Address() {}

Address::Address(const Address& address) : FormGroup() {
  *this = address;
}

Address::~Address() {}

Address& Address::operator=(const Address& address) {
  if (this == &address)
    return *this;

  line1_ = address.line1_;
  line2_ = address.line2_;
  city_ = address.city_;
  state_ = address.state_;
  country_code_ = address.country_code_;
  zip_code_ = address.zip_code_;
  return *this;
}

void Address::GetSupportedTypes(FieldTypeSet* supported_types) const {
  supported_types->insert(ADDRESS_HOME_LINE1);
  supported_types->insert(ADDRESS_HOME_LINE2);
  supported_types->insert(ADDRESS_HOME_CITY);
  supported_types->insert(ADDRESS_HOME_STATE);
  supported_types->insert(ADDRESS_HOME_ZIP);
  supported_types->insert(ADDRESS_HOME_COUNTRY);
}

base::string16 Address::GetRawInfo(AutofillFieldType type) const {
  type = AutofillType::GetEquivalentFieldType(type);
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
    return country_code_;

  return base::string16();
}

void Address::SetRawInfo(AutofillFieldType type, const base::string16& value) {
  type = AutofillType::GetEquivalentFieldType(type);
  if (type == ADDRESS_HOME_LINE1) {
    line1_ = value;
  } else if (type == ADDRESS_HOME_LINE2) {
    line2_ = value;
  } else if (type == ADDRESS_HOME_CITY) {
    city_ = value;
  } else if (type == ADDRESS_HOME_STATE) {
    state_ = value;
  } else if (type == ADDRESS_HOME_COUNTRY) {
    DCHECK(value.empty() || value.length() == 2u);
    country_code_ = value;
  } else if (type == ADDRESS_HOME_ZIP) {
    zip_code_ = value;
  } else {
    NOTREACHED();
  }
}

base::string16 Address::GetInfo(AutofillFieldType type,
                                const std::string& app_locale) const {
  type = AutofillType::GetEquivalentFieldType(type);
  if (type == ADDRESS_HOME_COUNTRY && !country_code_.empty())
    return AutofillCountry(UTF16ToASCII(country_code_), app_locale).name();

  return GetRawInfo(type);
}

bool Address::SetInfo(AutofillFieldType type,
                      const base::string16& value,
                      const std::string& app_locale) {
  type = AutofillType::GetEquivalentFieldType(type);
  if (type == ADDRESS_HOME_COUNTRY && !value.empty()) {
    country_code_ =
        ASCIIToUTF16(AutofillCountry::GetCountryCode(value, app_locale));
    return !country_code_.empty();
  }

  SetRawInfo(type, value);
  return true;
}

void Address::GetMatchingTypes(const base::string16& text,
                               const std::string& app_locale,
                               FieldTypeSet* matching_types) const {
  FormGroup::GetMatchingTypes(text, app_locale, matching_types);

  // Check to see if the |text| canonicalized as a country name is a match.
  std::string country_code = AutofillCountry::GetCountryCode(text, app_locale);
  if (!country_code.empty() && country_code_ == ASCIIToUTF16(country_code))
    matching_types->insert(ADDRESS_HOME_COUNTRY);
}

}  // namespace autofill
