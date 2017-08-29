// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_details_modifier.h"

#include "base/memory/ptr_util.h"
#include "base/values.h"

namespace payments {

namespace {

// These are defined as part of the spec at:
// https://w3c.github.io/payment-request/#dom-paymentdetailsmodifier
static const char kPaymentDetailsModifierTotal[] = "total";
static const char kPaymentDetailsModifierSupportedMethods[] =
    "supportedMethods";

}  // namespace

PaymentDetailsModifier::PaymentDetailsModifier() {}
PaymentDetailsModifier::PaymentDetailsModifier(
    const PaymentDetailsModifier& other) = default;
PaymentDetailsModifier::~PaymentDetailsModifier() = default;

bool PaymentDetailsModifier::operator==(
    const PaymentDetailsModifier& other) const {
  return this->supported_methods == other.supported_methods &&
         this->total == other.total &&
         this->additional_display_items == other.additional_display_items;
}

bool PaymentDetailsModifier::operator!=(
    const PaymentDetailsModifier& other) const {
  return !(*this == other);
}

std::unique_ptr<base::DictionaryValue>
PaymentDetailsModifier::ToDictionaryValue() const {
  std::unique_ptr<base::DictionaryValue> result =
      base::MakeUnique<base::DictionaryValue>();

  std::unique_ptr<base::ListValue> methods =
      base::MakeUnique<base::ListValue>();
  size_t numMethods = this->supported_methods.size();
  for (size_t i = 0; i < numMethods; i++) {
    methods->GetList().emplace_back(this->supported_methods[i]);
  }
  result->SetList(kPaymentDetailsModifierSupportedMethods, std::move(methods));
  result->SetDictionary(kPaymentDetailsModifierTotal,
                        this->total.ToDictionaryValue());

  return result;
}

}  // namespace payments
