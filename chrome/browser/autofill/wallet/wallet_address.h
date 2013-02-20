// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_WALLET_WALLET_ADDRESS_H_
#define CHROME_BROWSER_AUTOFILL_WALLET_WALLET_ADDRESS_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/string16.h"
#include "chrome/browser/autofill/field_types.h"

namespace base {
class DictionaryValue;
}

namespace wallet {

// TODO(ahutter): This address is a lot like chrome/browser/autofill/address.h.
// There should be a super class that both extend from to clean up duplicated
// code. See http://crbug.com/164463.

// Address contains various address fields that have been populated from the
// user's Online Wallet. It is loosely modeled as a subet of the OASIS
// "extensible Address Language" (xAL); see
// http://www.oasis-open.org/committees/ciq/download.shtml.
class Address {
 public:
  Address();
  // TODO(ahutter): Use additional fields (descriptive_name, is_post_box,
  // is_minimal_address, is_valid, is_default) when SaveToWallet is implemented.
  // See http://crbug.com/164284.
  Address(const std::string& country_name_code,
          const string16& recipient_name,
          const string16& address_line_1,
          const string16& address_line_2,
          const string16& locality_name,
          const string16& administrative_area_name,
          const string16& postal_code_number,
          const string16& phone_number,
          const std::string& object_id);
  ~Address();
  const std::string& country_name_code() const { return country_name_code_; }
  const string16& recipient_name() const { return recipient_name_; }
  const string16& address_line_1() const { return address_line_1_; }
  const string16& address_line_2() const { return address_line_2_; }
  const string16& locality_name() const { return locality_name_; }
  const string16& admin_area_name() const {
    return administrative_area_name_;
  }
  const string16& postal_code_number() const { return postal_code_number_; }
  const string16& phone_number() const { return phone_number_; }
  const std::string& object_id() const { return object_id_; }

  void set_country_name_code(const std::string& country_name_code) {
    country_name_code_ = country_name_code;
  }
  void set_recipient_name(const string16& recipient_name) {
    recipient_name_ = recipient_name;
  }
  void set_address_line_1(const string16& address_line_1) {
    address_line_1_ = address_line_1;
  }
  void set_address_line_2(const string16& address_line_2) {
    address_line_2_ = address_line_2;
  }
  void set_locality_name(const string16& locality_name) {
    locality_name_ = locality_name;
  }
  void set_admin_area_name(const string16& administrative_area_name) {
    administrative_area_name_ = administrative_area_name;
  }
  void set_postal_code_number(const string16& postal_code_number) {
    postal_code_number_ = postal_code_number;
  }
  void set_phone_number(const string16& phone_number) {
    phone_number_ = phone_number;
  }
  void set_object_id(const std::string& object_id) {
    object_id_ = object_id;
  }

  // If an address is being upgraded, it will be sent to the server in a
  // different format and with a few additional fields set, most importantly
  // |object_id_|.
  scoped_ptr<base::DictionaryValue> ToDictionaryWithID() const;

  // Newly created addresses will not have an associated |object_id_| and are
  // sent to the server in a slightly different format.
  scoped_ptr<base::DictionaryValue> ToDictionaryWithoutID() const;

  // Returns a string that summarizes this address, suitable for display to
  // the user.
  string16 DisplayName() const;

  // Returns data appropriate for |type|.
  string16 GetInfo(AutofillFieldType type) const;

  // Returns an empty scoped_ptr if input is invalid or a valid address that is
  // selectable for Google Wallet use.
  static scoped_ptr<Address>
      CreateAddressWithID(const base::DictionaryValue& dictionary);

  // Returns an empty scoped_ptr if input in invalid or a valid address that
  // can only be used for displaying to the user.
  static scoped_ptr<Address>
      CreateDisplayAddress(const base::DictionaryValue& dictionary);

  bool operator==(const Address& other) const;
  bool operator!=(const Address& other) const;

 private:
  // |country_name_code_| should be an ISO 3166-1-alpha-2 (two letter codes, as
  // used in DNS). For example, "GB".
  std::string country_name_code_;

  // The recipient's name. For example "John Doe".
  string16 recipient_name_;

  // |address_line_1| and |address_line_2| correspond to the "AddressLine"
  // elements in xAL, which are used to hold unstructured text.
  string16 address_line_1_;
  string16 address_line_2_;

  // Locality.  This is something of a fuzzy term, but it generally refers to
  // the city/town portion of an address.  In regions of the world where
  // localities are not well defined or do not fit into this structure well
  // (for example, Japan and China), leave locality_name empty and use
  // |address_line_2|.
  // Examples: US city, IT comune, UK post town.
  string16 locality_name_;

  // Top-level administrative subdivision of this country.
  // Examples: US state, IT region, UK constituent nation, JP prefecture.
  string16 administrative_area_name_;

  // Despite the name, |postal_code_number_| values are frequently alphanumeric.
  // Examples: "94043", "SW1W", "SW1W 9TQ".
  string16 postal_code_number_;

  // A valid international phone number. If |phone_number_| is a user provided
  // value, it should have been validated using libphonenumber by clients of
  // this class before being set; see http://code.google.com/p/libphonenumber/.
  string16 phone_number_;

  // Externalized Online Wallet id for this address.
  std::string object_id_;

  DISALLOW_COPY_AND_ASSIGN(Address);
};

}  // namespace wallet

#endif  // CHROME_BROWSER_AUTOFILL_WALLET_WALLET_ADDRESS_H_

