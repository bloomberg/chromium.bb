// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_PAYMENT_ADDRESS_H_
#define COMPONENTS_PAYMENTS_CORE_PAYMENT_ADDRESS_H_

#include <memory>
#include <vector>

#include "base/strings/string16.h"

// C++ bindings for the PaymentRequest API PaymentAddress. Conforms to the
// following spec:
// https://w3c.github.io/browser-payment-api/#paymentaddress-interface

namespace base {
class DictionaryValue;
}

namespace payments {

// A shipping or billing address.
struct PaymentAddress {
 public:
  PaymentAddress();
  PaymentAddress(const PaymentAddress& other);
  ~PaymentAddress();

  bool operator==(const PaymentAddress& other) const;
  bool operator!=(const PaymentAddress& other) const;

  // Populates |value| with the properties of this PaymentAddress.
  std::unique_ptr<base::DictionaryValue> ToDictionaryValue() const;

  // The CLDR (Common Locale Data Repository) region code. For example, US, GB,
  // CN, or JP.
  base::string16 country;

  // The most specific part of the address. It can include, for example, a
  // street name, a house number, apartment number, a rural delivery route,
  // descriptive instructions, or a post office box number.
  std::vector<base::string16> address_line;

  // The top level administrative subdivision of the country. For example, this
  // can be a state, a province, an oblast, or a prefecture.
  base::string16 region;

  // The city/town portion of the address.
  base::string16 city;

  // The dependent locality or sublocality within a city. For example, used for
  // neighborhoods, boroughs, districts, or UK dependent localities.
  base::string16 dependent_locality;

  // The postal code or ZIP code, also known as PIN code in India.
  base::string16 postal_code;

  // The sorting code as used in, for example, France.
  base::string16 sorting_code;

  // The BCP-47 language code for the address. It's used to determine the field
  // separators and the order of fields when formatting the address for display.
  base::string16 language_code;

  // The organization, firm, company, or institution at this address.
  base::string16 organization;

  // The name of the recipient or contact person.
  base::string16 recipient;

  // The phone number of the recipient or contact person.
  base::string16 phone;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_PAYMENT_ADDRESS_H_
