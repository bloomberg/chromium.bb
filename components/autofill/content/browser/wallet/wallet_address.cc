// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/wallet/wallet_address.h"

#include "base/logging.h"
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

  base::string16 phone_number;
  if (!dictionary.GetString("phone_number", &phone_number))
    DVLOG(1) << "Response from Google Wallet missing phone number";

  base::string16 address_line_1;
  base::string16 address_line_2;
  const ListValue* address_line_list;
  if (dictionary.GetList("postal_address.address_line", &address_line_list)) {
    if (!address_line_list->GetString(0, &address_line_1))
      DVLOG(1) << "Response from Google Wallet missing address line 1";
    if (!address_line_list->GetString(1, &address_line_2))
      DVLOG(1) << "Response from Google Wallet missing address line 2";
  } else {
    DVLOG(1) << "Response from Google Wallet missing address lines";
  }

  base::string16 locality_name;
  if (!dictionary.GetString("postal_address.locality_name",
                            &locality_name)) {
    DVLOG(1) << "Response from Google Wallet missing locality name";
  }

  base::string16 administrative_area_name;
  if (!dictionary.GetString("postal_address.administrative_area_name",
                            &administrative_area_name)) {
    DVLOG(1) << "Response from Google Wallet missing administrative area name";
  }

  Address* address = new Address(country_name_code,
                                 recipient_name,
                                 address_line_1,
                                 address_line_2,
                                 locality_name,
                                 administrative_area_name,
                                 postal_code_number,
                                 phone_number,
                                 object_id);

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
          UTF16ToASCII(profile.GetRawInfo(ADDRESS_HOME_COUNTRY))),
      recipient_name_(profile.GetRawInfo(NAME_FULL)),
      address_line_1_(profile.GetRawInfo(ADDRESS_HOME_LINE1)),
      address_line_2_(profile.GetRawInfo(ADDRESS_HOME_LINE2)),
      locality_name_(profile.GetRawInfo(ADDRESS_HOME_CITY)),
      postal_code_number_(profile.GetRawInfo(ADDRESS_HOME_ZIP)),
      phone_number_(profile.GetRawInfo(PHONE_HOME_WHOLE_NUMBER)),
      is_complete_address_(true) {
  state_names::GetNameAndAbbreviation(profile.GetRawInfo(ADDRESS_HOME_STATE),
                                      NULL,
                                      &administrative_area_name_);
  StringToUpperASCII(&administrative_area_name_);

  if (!country_name_code_.empty())
    phone_object_ = i18n::PhoneObject(phone_number_, country_name_code_);
}

Address::Address(const std::string& country_name_code,
                 const base::string16& recipient_name,
                 const base::string16& address_line_1,
                 const base::string16& address_line_2,
                 const base::string16& locality_name,
                 const base::string16& administrative_area_name,
                 const base::string16& postal_code_number,
                 const base::string16& phone_number,
                 const std::string& object_id)
    : country_name_code_(country_name_code),
      recipient_name_(recipient_name),
      address_line_1_(address_line_1),
      address_line_2_(address_line_2),
      locality_name_(locality_name),
      administrative_area_name_(administrative_area_name),
      postal_code_number_(postal_code_number),
      phone_number_(phone_number),
      phone_object_(phone_number, country_name_code),
      object_id_(object_id),
      is_complete_address_(true) {}

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

  base::string16 address1;
  if (!dictionary.GetString("address1", &address1))
    DVLOG(1) << "Reponse from Google Wallet missing address1";

  base::string16 address2;
  if (!dictionary.GetString("address2", &address2))
    DVLOG(1) << "Reponse from Google Wallet missing address2";

  base::string16 city;
  if (!dictionary.GetString("city", &city))
    DVLOG(1) << "Reponse from Google Wallet missing city";

  base::string16 state;
  if (!dictionary.GetString("state", &state))
    DVLOG(1) << "Reponse from Google Wallet missing state";

  base::string16 phone_number;
  if (!dictionary.GetString("phone_number", &phone_number))
    DVLOG(1) << "Reponse from Google Wallet missing phone number";

  std::string address_state;
  if (!dictionary.GetString("type", &address_state))
    DVLOG(1) << "Response from Google Wallet missing type/state of address";

  scoped_ptr<Address> address(
      new Address(country_code,
                  name,
                  address1,
                  address2,
                  city,
                  state,
                  postal_code,
                  phone_number,
                  std::string()));
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
  address_lines->AppendString(address_line_1_);
  if (!address_line_2_.empty())
    address_lines->AppendString(address_line_2_);
  dict->Set("address_line", address_lines.release());

  dict->SetString("country_name_code", country_name_code_);
  dict->SetString("recipient_name", recipient_name_);
  dict->SetString("locality_name", locality_name_);
  dict->SetString("administrative_area_name",
                  administrative_area_name_);
  dict->SetString("postal_code_number", postal_code_number_);

  return dict.Pass();
}

base::string16 Address::DisplayName() const {
#if defined(OS_ANDROID)
  // TODO(aruslan): improve this stub implementation.
  return recipient_name();
#else
  // TODO(estade): improve this stub implementation + l10n.
  return recipient_name() + ASCIIToUTF16(", ") + address_line_1();
#endif
}

base::string16 Address::DisplayNameDetail() const {
#if defined(OS_ANDROID)
  // TODO(aruslan): improve this stub implementation.
  return address_line_1();
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
    DCHECK(IsStringASCII(country_name_code()));
    return ASCIIToUTF16(country_name_code());
  } else if (type.html_type() == HTML_TYPE_STREET_ADDRESS) {
    base::string16 address = address_line_1();
    if (!address_line_2().empty())
      address += ASCIIToUTF16(", ") + address_line_2();
    return address;
  }

  switch (type.GetStorableType()) {
    case NAME_FULL:
      return recipient_name();

    case ADDRESS_HOME_LINE1:
      return address_line_1();

    case ADDRESS_HOME_LINE2:
      return address_line_2();

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
    case ADDRESS_HOME_SORTING_CODE:
    case COMPANY_NAME:
      // Fields that some countries request but Wallet doesn't support.
      // TODO(dbeam): can these be supported by Wallet?
      return base::string16();

    // TODO(estade): implement more.
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
         address_line_1_ == other.address_line_1_ &&
         address_line_2_ == other.address_line_2_ &&
         locality_name_ == other.locality_name_ &&
         administrative_area_name_ == other.administrative_area_name_ &&
         postal_code_number_ == other.postal_code_number_ &&
         phone_number_ == other.phone_number_ &&
         is_complete_address_ == other.is_complete_address_;
}

bool Address::operator==(const Address& other) const {
  return object_id_ == other.object_id_ && EqualsIgnoreID(other);
}

bool Address::operator!=(const Address& other) const {
  return !(*this == other);
}

}  // namespace wallet
}  // namespace autofill
