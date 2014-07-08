// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/address_i18n.h"

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "third_party/libaddressinput/chromium/cpp/include/libaddressinput/address_data.h"

namespace autofill {
namespace i18n {

namespace {

base::string16 GetInfoHelper(const AutofillProfile& profile,
                             const std::string& app_locale,
                             const AutofillType& type) {
  return profile.GetInfo(type, app_locale);
}

}  // namespace

using ::i18n::addressinput::AddressData;

scoped_ptr<AddressData> CreateAddressData(
    const base::Callback<base::string16(const AutofillType&)>& get_info) {
  scoped_ptr<AddressData> address_data(new AddressData());
  address_data->recipient = base::UTF16ToUTF8(
      get_info.Run(AutofillType(NAME_FULL)));
  address_data->region_code = base::UTF16ToUTF8(
      get_info.Run(AutofillType(HTML_TYPE_COUNTRY_CODE, HTML_MODE_NONE)));
  address_data->administrative_area = base::UTF16ToUTF8(
      get_info.Run(AutofillType(ADDRESS_HOME_STATE)));
  address_data->locality = base::UTF16ToUTF8(
      get_info.Run(AutofillType(ADDRESS_HOME_CITY)));
  address_data->dependent_locality = base::UTF16ToUTF8(
      get_info.Run(AutofillType(ADDRESS_HOME_DEPENDENT_LOCALITY)));
  address_data->sorting_code = base::UTF16ToUTF8(
      get_info.Run(AutofillType(ADDRESS_HOME_SORTING_CODE)));
  address_data->postal_code = base::UTF16ToUTF8(
      get_info.Run(AutofillType(ADDRESS_HOME_ZIP)));
  base::SplitString(
      base::UTF16ToUTF8(
          get_info.Run(AutofillType(ADDRESS_HOME_STREET_ADDRESS))),
      '\n',
      &address_data->address_line);
  return address_data.Pass();
}

scoped_ptr< ::i18n::addressinput::AddressData>
    CreateAddressDataFromAutofillProfile(const AutofillProfile& profile,
                                         const std::string& app_locale) {
  scoped_ptr< ::i18n::addressinput::AddressData> address_data =
      i18n::CreateAddressData(base::Bind(&GetInfoHelper, profile, app_locale));
  address_data->language_code = profile.language_code();
  return address_data.Pass();
}

}  // namespace i18n
}  // namespace autofill
