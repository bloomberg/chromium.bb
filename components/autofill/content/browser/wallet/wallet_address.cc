// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/wallet/wallet_address.h"

#include "base/logging.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/autofill/core/browser/autofill_country.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/phone_number.h"
#include "components/autofill/core/browser/state_names.h"

namespace autofill {
namespace wallet {

// Server specified type for address with complete details.
const char kFullAddress[] = "FULL";

namespace {

Address* CreateAddressInternal(const base::DictionaryValue& dictionary,
                               const std::string& object_id) {
  std::string country_name_code;
  if (!dictionary.GetString("postal_address.country_name_code",
                            &country_name_code)) {
    DLOG(ERROR) << "Response from Google Wallet missing country name";
    return NULL;
  }

  base::string16 recipient_name;
  if (!dictionary.GetString("postal_address.recipient_name",
                            &recipient_name)) {
    DLOG(ERROR) << "Response from Google Wallet missing recipient name";
    return NULL;
  }

  base::string16 postal_code_number;
  if (!dictionary.GetString("postal_address.postal_code_number",
                            &postal_code_number)) {
    DLOG(ERROR) << "Response from Google Wallet missing postal code number";
    return NULL;
  }
  // TODO(estade): what about postal_code_number_extension?

  base::string16 sorting_code;
  if (!dictionary.GetString("postal_address.sorting_code",
                            &sorting_code)) {
    DVLOG(1) << "Response from Google Wallet missing sorting code";
  }

  base::string16 phone_number;
  if (!dictionary.GetString("phone_number", &phone_number))
    DVLOG(1) << "Response from Google Wallet missing phone number";

  std::vector<base::string16> street_address;
  const base::ListValue* address_line_list;
  if (dictionary.GetList("postal_address.address_line", &address_line_list)) {
    for (size_t i = 0; i < address_line_list->GetSize(); ++i) {
      base::string16 line;
      address_line_list->GetString(i, &line);
      street_address.push_back(line);
    }
  } else {
    DVLOG(1) << "Response from Google Wallet missing address lines";
  }

  base::string16 locality_name;
  if (!dictionary.GetString("postal_address.locality_name",
                            &locality_name)) {
    DVLOG(1) << "Response from Google Wallet missing locality name";
  }

  base::string16 dependent_locality_name;
  if (!dictionary.GetString("postal_address.dependent_locality_name",
                            &dependent_locality_name)) {
    DVLOG(1) << "Response from Google Wallet missing dependent locality name";
  }

  base::string16 administrative_area_name;
  if (!dictionary.GetString("postal_address.administrative_area_name",
                            &administrative_area_name)) {
    DVLOG(1) << "Response from Google Wallet missing administrative area name";
  }

  std::string language_code;
  if (!dictionary.GetString("postal_address.language_code",
                            &language_code)) {
    DVLOG(1) << "Response from Google Wallet missing language code";
  }

  Address* address = new Address(country_name_code,
                                 recipient_name,
                                 street_address,
                                 locality_name,
                                 dependent_locality_name,
                                 administrative_area_name,
                                 postal_code_number,
                                 sorting_code,
                                 phone_number,
                                 object_id,
                                 language_code);

  bool is_minimal_address = false;
  if (dictionary.GetBoolean("is_minimal_address", &is_minimal_address))
    address->set_is_complete_address(!is_minimal_address);
  else
    DVLOG(1) << "Response from Google Wallet missing is_minimal_address bit";

  return address;
}

}  // namespace

Address::Address() {}

Address::Address(const AutofillProfile& profile)
    : country_name_code_(
          base::UTF16ToASCII(profile.GetRawInfo(ADDRESS_HOME_COUNTRY))),
      recipient_name_(profile.GetRawInfo(NAME_FULL)),
      locality_name_(profile.GetRawInfo(ADDRESS_HOME_CITY)),
      dependent_locality_name_(
          profile.GetRawInfo(ADDRESS_HOME_DEPENDENT_LOCALITY)),
      administrative_area_name_(profile.GetRawInfo(ADDRESS_HOME_STATE)),
      postal_code_number_(profile.GetRawInfo(ADDRESS_HOME_ZIP)),
      sorting_code_(profile.GetRawInfo(ADDRESS_HOME_SORTING_CODE)),
      phone_number_(profile.GetRawInfo(PHONE_HOME_WHOLE_NUMBER)),
      is_complete_address_(true),
      language_code_(profile.language_code()) {
  base::SplitString(
      profile.GetRawInfo(ADDRESS_HOME_STREET_ADDRESS), '\n', &street_address_);

  if (!country_name_code_.empty())
    phone_object_ = i18n::PhoneObject(phone_number_, country_name_code_);
}

Address::Address(const std::string& country_name_code,
                 const base::string16& recipient_name,
                 const std::vector<base::string16>& street_address,
                 const base::string16& locality_name,
                 const base::string16& dependent_locality_name,
                 const base::string16& administrative_area_name,
                 const base::string16& postal_code_number,
                 const base::string16& sorting_code,
                 const base::string16& phone_number,
                 const std::string& object_id,
                 const std::string& language_code)
    : country_name_code_(country_name_code),
      recipient_name_(recipient_name),
      street_address_(street_address),
      locality_name_(locality_name),
      dependent_locality_name_(dependent_locality_name),
      administrative_area_name_(administrative_area_name),
      postal_code_number_(postal_code_number),
      sorting_code_(sorting_code),
      phone_number_(phone_number),
      phone_object_(phone_number, country_name_code),
      object_id_(object_id),
      is_complete_address_(true),
      language_code_(language_code) {}

Address::~Address() {}

// static
scoped_ptr<Address> Address::CreateAddressWithID(
    const base::DictionaryValue& dictionary) {
  std::string object_id;
  if (!dictionary.GetString("id", &object_id)) {
    DLOG(ERROR) << "Response from Google Wallet missing object id";
    return scoped_ptr<Address>();
  }
  return scoped_ptr<Address>(CreateAddressInternal(dictionary, object_id));
}

// static
scoped_ptr<Address> Address::CreateAddress(
    const base::DictionaryValue& dictionary) {
  std::string object_id;
  dictionary.GetString("id", &object_id);
  return scoped_ptr<Address>(CreateAddressInternal(dictionary, object_id));
}

// static
scoped_ptr<Address> Address::CreateDisplayAddress(
    const base::DictionaryValue& dictionary) {
  std::string country_code;
  if (!dictionary.GetString("country_code", &country_code)) {
    DLOG(ERROR) << "Reponse from Google Wallet missing country code";
    return scoped_ptr<Address>();
  }

  base::string16 name;
  if (!dictionary.GetString("name", &name)) {
    DLOG(ERROR) << "Reponse from Google Wallet missing name";
    return scoped_ptr<Address>();
  }

  base::string16 postal_code;
  if (!dictionary.GetString("postal_code", &postal_code)) {
    DLOG(ERROR) << "Reponse from Google Wallet missing postal code";
    return scoped_ptr<Address>();
  }

  base::string16 sorting_code;
  if (!dictionary.GetString("sorting_code", &sorting_code)) {
    DVLOG(1) << "Reponse from Google Wallet missing sorting code";
  }

  std::vector<base::string16> street_address;
  base::string16 address1;
  if (dictionary.GetString("address1", &address1))
    street_address.push_back(address1);
  else
    DVLOG(1) << "Reponse from Google Wallet missing address1";

  base::string16 address2;
  if (dictionary.GetString("address2", &address2) && !address2.empty()) {
    street_address.resize(2);
    street_address[1] = address2;
  } else {
    DVLOG(1) << "Reponse from Google Wallet missing or empty address2";
  }

  base::string16 city;
  if (!dictionary.GetString("city", &city))
    DVLOG(1) << "Reponse from Google Wallet missing city";

  base::string16 dependent_locality_name;
  if (!dictionary.GetString("dependent_locality_name",
                            &dependent_locality_name)) {
    DVLOG(1) << "Reponse from Google Wallet missing district";
  }

  base::string16 state;
  if (!dictionary.GetString("state", &state))
    DVLOG(1) << "Reponse from Google Wallet missing state";

  base::string16 phone_number;
  if (!dictionary.GetString("phone_number", &phone_number))
    DVLOG(1) << "Reponse from Google Wallet missing phone number";

  std::string address_state;
  if (!dictionary.GetString("type", &address_state))
    DVLOG(1) << "Response from Google Wallet missing type/state of address";

  std::string language_code;
  if (!dictionary.GetString("language_code", &language_code))
    DVLOG(1) << "Response from Google Wallet missing language code";

  scoped_ptr<Address> address(
      new Address(country_code,
                  name,
                  street_address,
                  city,
                  dependent_locality_name,
                  state,
                  postal_code,
                  sorting_code,
                  phone_number,
                  std::string(),
                  language_code));
  address->set_is_complete_address(address_state == kFullAddress);

  return address.Pass();
}

scoped_ptr<base::DictionaryValue> Address::ToDictionaryWithID() const {
  scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue());

  if (!object_id_.empty())
    dict->SetString("id", object_id_);
  dict->SetString("phone_number", phone_number_);
  dict->Set("postal_address", ToDictionaryWithoutID().release());

  return dict.Pass();
}

scoped_ptr<base::DictionaryValue> Address::ToDictionaryWithoutID() const {
  scoped_ptr<base::DictionaryValue> dict(new base::DictionaryValue());

  scoped_ptr<base::ListValue> address_lines(new base::ListValue());
  address_lines->AppendStrings(street_address_);
  dict->Set("address_line", address_lines.release());

  dict->SetString("country_name_code", country_name_code_);
  dict->SetString("recipient_name", recipient_name_);
  dict->SetString("locality_name", locality_name_);
  dict->SetString("dependent_locality_name", dependent_locality_name_);
  dict->SetString("administrative_area_name",
                  administrative_area_name_);
  dict->SetString("postal_code_number", postal_code_number_);
  dict->SetString("sorting_code", sorting_code_);
  dict->SetString("language_code", language_code_);

  return dict.Pass();
}

base::string16 Address::DisplayName() const {
#if defined(OS_ANDROID)
  // TODO(aruslan): improve this stub implementation.
  return recipient_name();
#else
  // TODO(estade): improve this stub implementation + l10n.
  return recipient_name() + base::ASCIIToUTF16(", ") + GetStreetAddressLine(0);
#endif
}

base::string16 Address::DisplayNameDetail() const {
#if defined(OS_ANDROID)
  // TODO(aruslan): improve this stub implementation.
  return GetStreetAddressLine(0);
#else
  return base::string16();
#endif
}

base::string16 Address::DisplayPhoneNumber() const {
  // Return a formatted phone number. Wallet doesn't store user formatting, so
  // impose our own. phone_number() always includes a country code, so using
  // PhoneObject to format it would result in an internationalized format. Since
  // Wallet only supports the US right now, stick to national formatting.
  return i18n::PhoneObject(phone_number(), country_name_code()).
      GetNationallyFormattedNumber();
}

base::string16 Address::GetInfo(const AutofillType& type,
                                const std::string& app_locale) const {
  if (type.html_type() == HTML_TYPE_COUNTRY_CODE) {
    DCHECK(base::IsStringASCII(country_name_code()));
    return base::ASCIIToUTF16(country_name_code());
  }

  switch (type.GetStorableType()) {
    case NAME_FULL:
      return recipient_name();

    case ADDRESS_HOME_STREET_ADDRESS:
      return JoinString(street_address_, base::ASCIIToUTF16("\n"));

    case ADDRESS_HOME_LINE1:
      return GetStreetAddressLine(0);

    case ADDRESS_HOME_LINE2:
      return GetStreetAddressLine(1);

    case ADDRESS_HOME_CITY:
      return locality_name();

    case ADDRESS_HOME_STATE:
      return administrative_area_name();

    case ADDRESS_HOME_ZIP:
      return postal_code_number();

    case ADDRESS_HOME_COUNTRY: {
      AutofillCountry country(country_name_code(), app_locale);
      return country.name();
    }

    case PHONE_HOME_WHOLE_NUMBER:
      // Wallet doesn't store user phone number formatting, so just strip all
      // formatting.
      return phone_object_.GetWholeNumber();

    case ADDRESS_HOME_DEPENDENT_LOCALITY:
      return dependent_locality_name_;

    case ADDRESS_HOME_SORTING_CODE:
      return sorting_code_;

    case COMPANY_NAME:
      // A field that Wallet doesn't support. TODO(dbeam): can it be supported?
      return base::string16();

    default:
      NOTREACHED();
      return base::string16();
  }
}

void Address::SetPhoneNumber(const base::string16& phone_number) {
  phone_number_ = phone_number;
  phone_object_ = i18n::PhoneObject(phone_number_, country_name_code_);
}

bool Address::EqualsIgnoreID(const Address& other) const {
  return country_name_code_ == other.country_name_code_ &&
         recipient_name_ == other.recipient_name_ &&
         street_address_ == other.street_address_ &&
         locality_name_ == other.locality_name_ &&
         dependent_locality_name_ == other.dependent_locality_name_ &&
         administrative_area_name_ == other.administrative_area_name_ &&
         postal_code_number_ == other.postal_code_number_ &&
         sorting_code_ == other.sorting_code_ &&
         phone_number_ == other.phone_number_ &&
         is_complete_address_ == other.is_complete_address_;
}

base::string16 Address::GetStreetAddressLine(size_t line) const {
  return street_address_.size() > line ? street_address_[line] :
                                         base::string16();
}

bool Address::operator==(const Address& other) const {
  return object_id_ == other.object_id_ &&
         language_code_ == other.language_code_ &&
         EqualsIgnoreID(other);
}

bool Address::operator!=(const Address& other) const {
  return !(*this == other);
}

}  // namespace wallet
}  // namespace autofill
