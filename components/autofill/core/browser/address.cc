// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/address.h"

#include <stddef.h>

#include "base/basictypes.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_country.h"
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/autofill_type.h"

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

base::string16 Address::GetRawInfo(ServerFieldType type) const {
  DCHECK_EQ(ADDRESS_HOME, AutofillType(type).group());
  switch (type) {
    case ADDRESS_HOME_LINE1:
      return line1_;

    case ADDRESS_HOME_LINE2:
      return line2_;

    case ADDRESS_HOME_CITY:
      return city_;

    case ADDRESS_HOME_STATE:
      return state_;

    case ADDRESS_HOME_ZIP:
      return zip_code_;

    case ADDRESS_HOME_COUNTRY:
      return ASCIIToUTF16(country_code_);

    case ADDRESS_HOME_STREET_ADDRESS: {
      base::string16 address = line1_;
      if (!line2_.empty())
        address += ASCIIToUTF16("\n") + line2_;
      return address;
    }

    // TODO(isherman): Add support for these field types in support of i18n.
    case ADDRESS_HOME_SORTING_CODE:
    case ADDRESS_HOME_DEPENDENT_LOCALITY:
      return base::string16();

    default:
      NOTREACHED();
      return base::string16();
  }
}

void Address::SetRawInfo(ServerFieldType type, const base::string16& value) {
  DCHECK_EQ(ADDRESS_HOME, AutofillType(type).group());
  switch (type) {
    case ADDRESS_HOME_LINE1:
      line1_ = value;
      break;

    case ADDRESS_HOME_LINE2:
      line2_ = value;
      break;

    case ADDRESS_HOME_CITY:
      city_ = value;
      break;

    case ADDRESS_HOME_STATE:
      state_ = value;
      break;

    case ADDRESS_HOME_COUNTRY:
      DCHECK(value.empty() ||
             (value.length() == 2u && IsStringASCII(value)));
      country_code_ = UTF16ToASCII(value);
      break;

    case ADDRESS_HOME_ZIP:
      zip_code_ = value;
      break;

    case ADDRESS_HOME_STREET_ADDRESS: {
      // Clear any stale values, which might or might not get overwritten below.
      line1_.clear();
      line2_.clear();

      std::vector<base::string16> lines;
      base::SplitString(value, char16('\n'), &lines);
      if (lines.size() > 0)
        line1_ = lines[0];
      if (lines.size() > 1)
        line2_ = lines[1];

      // TODO(isherman): Add support for additional address lines.
      break;
    }

    // TODO(isherman): Add support for these field types in support of i18n.
    case ADDRESS_HOME_SORTING_CODE:
    case ADDRESS_HOME_DEPENDENT_LOCALITY:
      break;

    default:
      NOTREACHED();
  }
}

base::string16 Address::GetInfo(const AutofillType& type,
                                const std::string& app_locale) const {
  if (type.html_type() == HTML_TYPE_COUNTRY_CODE)
    return ASCIIToUTF16(country_code_);

  ServerFieldType storable_type = type.GetStorableType();
  if (storable_type == ADDRESS_HOME_COUNTRY && !country_code_.empty())
    return AutofillCountry(country_code_, app_locale).name();

  if (storable_type == ADDRESS_HOME_STREET_ADDRESS) {
    // TODO(isherman): Remove this special-case.
    base::string16 address = line1_;
    if (!line2_.empty())
      address += ASCIIToUTF16(", ") + line2_;
    return address;
  }

  return GetRawInfo(storable_type);
}

bool Address::SetInfo(const AutofillType& type,
                      const base::string16& value,
                      const std::string& app_locale) {
  if (type.html_type() == HTML_TYPE_COUNTRY_CODE) {
    if (!value.empty() && (value.size() != 2u || !IsStringASCII(value))) {
      country_code_ = std::string();
      return false;
    }

    country_code_ = StringToUpperASCII(UTF16ToASCII(value));
    return true;
  }

  ServerFieldType storable_type = type.GetStorableType();
  if (storable_type == ADDRESS_HOME_COUNTRY && !value.empty()) {
    country_code_ = AutofillCountry::GetCountryCode(value, app_locale);
    return !country_code_.empty();
  }

  // If the address doesn't have any newlines, don't attempt to parse it into
  // lines, since this is potentially a user-entered address in the user's own
  // format, so the code would have to rely on iffy heuristics at best.
  // Instead, just give up when importing addresses like this.
  if (storable_type == ADDRESS_HOME_STREET_ADDRESS && !value.empty() &&
      value.find(char16('\n')) == base::string16::npos) {
    line1_ = line2_ = base::string16();
    return false;
  }

  SetRawInfo(storable_type, value);
  return true;
}

void Address::GetMatchingTypes(const base::string16& text,
                               const std::string& app_locale,
                               ServerFieldTypeSet* matching_types) const {
  FormGroup::GetMatchingTypes(text, app_locale, matching_types);

  // Check to see if the |text| canonicalized as a country name is a match.
  std::string country_code = AutofillCountry::GetCountryCode(text, app_locale);
  if (!country_code.empty() && country_code_ == country_code)
    matching_types->insert(ADDRESS_HOME_COUNTRY);
}

void Address::GetSupportedTypes(ServerFieldTypeSet* supported_types) const {
  supported_types->insert(ADDRESS_HOME_LINE1);
  supported_types->insert(ADDRESS_HOME_LINE2);
  supported_types->insert(ADDRESS_HOME_CITY);
  supported_types->insert(ADDRESS_HOME_STATE);
  supported_types->insert(ADDRESS_HOME_ZIP);
  supported_types->insert(ADDRESS_HOME_COUNTRY);
}

}  // namespace autofill
