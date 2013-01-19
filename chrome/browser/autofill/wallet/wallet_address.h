// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_WALLET_WALLET_ADDRESS_H_
#define CHROME_BROWSER_AUTOFILL_WALLET_WALLET_ADDRESS_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

namespace base {
class DictionaryValue;
}

namespace wallet {

// TODO(ahutter): This address is a lot like chrome/browser/autofill/address.h.
// There should be a super class that both extend from to clean up duplicated
// code. See http://crbug.com/164463.

// Address contains various address fields that have been populated from the
// user's Online Wallet.
class Address {
 public:
  Address();
  // TODO(ahutter): Use additional fields (descriptive_name, is_post_box,
  // is_minimal_address, is_valid, is_default) when SaveToWallet is implemented.
  // See http://crbug.com/164284.
  Address(const std::string& country_name_code,
          const std::string& recipient_name,
          const std::string& address_line_1,
          const std::string& address_line_2,
          const std::string& locality_name,
          const std::string& administrative_area_name,
          const std::string& postal_code_number,
          const std::string& phone_number,
          const std::string& object_id);
  ~Address();
  const std::string& country_name_code() const { return country_name_code_; }
  const std::string& recipient_name() const { return recipient_name_; }
  const std::string& address_line_1() const { return address_line_1_; }
  const std::string& address_line_2() const { return address_line_2_; }
  const std::string& locality_name() const { return locality_name_; }
  const std::string& admin_area_name() const {
    return administrative_area_name_;
  }
  const std::string& postal_code_number() const { return postal_code_number_; }
  const std::string& phone_number() const { return phone_number_; }
  const std::string& object_id() const { return object_id_; }

  void set_country_name_code(const std::string& country_name_code) {
    country_name_code_ = country_name_code;
  }
  void set_recipient_name(const std::string& recipient_name) {
    recipient_name_ = recipient_name;
  }
  void set_address_line_1(const std::string& address_line_1) {
    address_line_1_ = address_line_1;
  }
  void set_address_line_2(const std::string& address_line_2) {
    address_line_2_ = address_line_2;
  }
  void set_locality_name(const std::string& locality_name) {
    locality_name_ = locality_name;
  }
  void set_admin_area_name(const std::string& administrative_area_name) {
    administrative_area_name_ = administrative_area_name;
  }
  void set_postal_code_number(const std::string& postal_code_number) {
    postal_code_number_ = postal_code_number;
  }
  void set_phone_number(const std::string& phone_number) {
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
  std::string country_name_code_;
  std::string recipient_name_;
  std::string address_line_1_;
  std::string address_line_2_;
  std::string locality_name_;
  std::string administrative_area_name_;
  std::string postal_code_number_;
  std::string phone_number_;
  std::string object_id_;
  DISALLOW_COPY_AND_ASSIGN(Address);
};

}  // namespace wallet

#endif  // CHROME_BROWSER_AUTOFILL_WALLET_WALLET_ADDRESS_H_

