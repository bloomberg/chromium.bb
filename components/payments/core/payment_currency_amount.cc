// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_currency_amount.h"

#include "base/values.h"

namespace payments {

namespace {

// These are defined as part of the spec at:
// https://w3c.github.io/browser-payment-api/#dom-paymentcurrencyamount
static const char kPaymentCurrencyAmountCurrencySystemISO4217[] =
    "urn:iso:std:iso:4217";
static const char kPaymentCurrencyAmountCurrencySystem[] = "currencySystem";
static const char kPaymentCurrencyAmountCurrency[] = "currency";
static const char kPaymentCurrencyAmountValue[] = "value";

}  // namespace

PaymentCurrencyAmount::PaymentCurrencyAmount()
    // By default, the currency is defined by [ISO4217]. For example, USD for
    // US Dollars.
    : currency_system(kPaymentCurrencyAmountCurrencySystemISO4217) {}

PaymentCurrencyAmount::~PaymentCurrencyAmount() = default;

bool PaymentCurrencyAmount::operator==(
    const PaymentCurrencyAmount& other) const {
  return currency == other.currency && value == other.value;
}

bool PaymentCurrencyAmount::operator!=(
    const PaymentCurrencyAmount& other) const {
  return !(*this == other);
}

bool PaymentCurrencyAmount::FromDictionaryValue(
    const base::DictionaryValue& dictionary_value) {
  if (!dictionary_value.GetString(kPaymentCurrencyAmountCurrency, &currency)) {
    return false;
  }

  if (!dictionary_value.GetString(kPaymentCurrencyAmountValue, &value)) {
    return false;
  }

  // Currency_system is optional
  dictionary_value.GetString(kPaymentCurrencyAmountCurrencySystem,
                             &currency_system);

  return true;
}

std::unique_ptr<base::DictionaryValue>
PaymentCurrencyAmount::ToDictionaryValue() const {
  auto result = std::make_unique<base::DictionaryValue>();
  result->SetString(kPaymentCurrencyAmountCurrency, currency);
  result->SetString(kPaymentCurrencyAmountValue, value);
  if (!currency_system.empty())
    result->SetString(kPaymentCurrencyAmountCurrencySystem, currency_system);

  return result;
}

}  // namespace payments
