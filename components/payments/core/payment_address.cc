// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_address.h"

#include "base/values.h"

namespace payments {

namespace {

// These are defined as part of the spec at:
// https://w3c.github.io/browser-payment-api/#paymentaddress-interface
static const char kAddressAddressLine[] = "addressLine";
static const char kAddressCity[] = "city";
static const char kAddressCountry[] = "country";
static const char kAddressDependentLocality[] = "dependentLocality";
static const char kAddressLanguageCode[] = "languageCode";
static const char kAddressOrganization[] = "organization";
static const char kAddressPhone[] = "phone";
static const char kAddressPostalCode[] = "postalCode";
static const char kAddressRecipient[] = "recipient";
static const char kAddressRegion[] = "region";
static const char kAddressSortingCode[] = "sortingCode";

}  // namespace

PaymentAddress::PaymentAddress() {}
PaymentAddress::PaymentAddress(const PaymentAddress& other) = default;
PaymentAddress::~PaymentAddress() = default;

bool PaymentAddress::operator==(const PaymentAddress& other) const {
  return country == other.country && address_line == other.address_line &&
         region == other.region && city == other.city &&
         dependent_locality == other.dependent_locality &&
         postal_code == other.postal_code &&
         sorting_code == other.sorting_code &&
         language_code == other.language_code &&
         organization == other.organization && recipient == other.recipient &&
         phone == other.phone;
}

bool PaymentAddress::operator!=(const PaymentAddress& other) const {
  return !(*this == other);
}

std::unique_ptr<base::DictionaryValue> PaymentAddress::ToDictionaryValue()
    const {
  auto result = std::make_unique<base::DictionaryValue>();
  result->SetString(kAddressCountry, country);
  auto address_line_list = std::make_unique<base::ListValue>();
  for (const base::string16& address_line_string : address_line) {
    if (!address_line_string.empty())
      address_line_list->AppendString(address_line_string);
  }
  result->Set(kAddressAddressLine, std::move(address_line_list));
  result->SetString(kAddressRegion, region);
  result->SetString(kAddressCity, city);
  result->SetString(kAddressDependentLocality, dependent_locality);
  result->SetString(kAddressPostalCode, postal_code);
  result->SetString(kAddressSortingCode, sorting_code);
  result->SetString(kAddressLanguageCode, language_code);
  result->SetString(kAddressOrganization, organization);
  result->SetString(kAddressRecipient, recipient);
  result->SetString(kAddressPhone, phone);

  return result;
}

}  // namespace payments
