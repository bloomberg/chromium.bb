// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_PAYMENT_CURRENCY_AMOUNT_H_
#define COMPONENTS_PAYMENTS_CORE_PAYMENT_CURRENCY_AMOUNT_H_

#include <memory>
#include <string>

// C++ bindings for the PaymentRequest API PaymentCurrencyAmount. Conforms to
// the following spec:
// https://w3c.github.io/browser-payment-api/#dom-paymentcurrencyamount

namespace base {
class DictionaryValue;
}

namespace payments {

// Supplies monetary amounts.
class PaymentCurrencyAmount {
 public:
  PaymentCurrencyAmount();
  ~PaymentCurrencyAmount();

  bool operator==(const PaymentCurrencyAmount& other) const;
  bool operator!=(const PaymentCurrencyAmount& other) const;

  // Populates the properties of this PaymentCurrencyAmount from |value|.
  // Returns true if the required values are present.
  bool FromDictionaryValue(const base::DictionaryValue& dictionary_value);

  // Creates a base::DictionaryValue with the properties of this
  // PaymentCurrencyAmount.
  std::unique_ptr<base::DictionaryValue> ToDictionaryValue() const;

  // A currency identifier. The most common identifiers are three-letter
  // alphabetic codes as defined by ISO 4217 (for example, "USD" for US Dollars)
  // however any string is considered valid.
  std::string currency;

  // A string containing the decimal monetary value.
  std::string value;

  // A URL that indicates the currency system that the currency identifier
  // belongs to.
  std::string currency_system;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_PAYMENT_CURRENCY_AMOUNT_H_
