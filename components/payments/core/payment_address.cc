// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_address.h"

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
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
  return this->country == other.country &&
         this->address_line == other.address_line &&
         this->region == other.region && this->city == other.city &&
         this->dependent_locality == other.dependent_locality &&
         this->postal_code == other.postal_code &&
         this->sorting_code == other.sorting_code &&
         this->language_code == other.language_code &&
         this->organization == other.organization &&
         this->recipient == other.recipient && this->phone == other.phone;
}

bool PaymentAddress::operator!=(const PaymentAddress& other) const {
  return !(*this == other);
}

std::unique_ptr<base::DictionaryValue> PaymentAddress::ToDictionaryValue()
    const {
  std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue());

  result->SetString(kAddressCountry, this->country);
  std::unique_ptr<base::ListValue> address_line =
      base::MakeUnique<base::ListValue>();
  for (const base::string16& address_line_string : this->address_line) {
    if (!address_line_string.empty())
      address_line->AppendString(address_line_string);
  }
  result->Set(kAddressAddressLine, std::move(address_line));
  result->SetString(kAddressRegion, this->region);
  result->SetString(kAddressCity, this->city);
  result->SetString(kAddressDependentLocality, this->dependent_locality);
  result->SetString(kAddressPostalCode, this->postal_code);
  result->SetString(kAddressSortingCode, this->sorting_code);
  result->SetString(kAddressLanguageCode, this->language_code);
  result->SetString(kAddressOrganization, this->organization);
  result->SetString(kAddressRecipient, this->recipient);
  result->SetString(kAddressPhone, this->phone);

  return result;
}

}  // namespace payments
