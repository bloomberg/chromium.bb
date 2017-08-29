// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_item.h"

#include "base/memory/ptr_util.h"
#include "base/values.h"

namespace payments {

namespace {

// These are defined as part of the spec at:
// https://w3c.github.io/payment-request/#dom-paymentitem
static const char kPaymentItemAmount[] = "amount";
static const char kPaymentItemLabel[] = "label";
static const char kPaymentItemPending[] = "pending";

}  // namespace

PaymentItem::PaymentItem() : pending(false) {}
PaymentItem::~PaymentItem() = default;

bool PaymentItem::operator==(const PaymentItem& other) const {
  return this->label == other.label && this->amount == other.amount &&
         this->pending == other.pending;
}

bool PaymentItem::operator!=(const PaymentItem& other) const {
  return !(*this == other);
}

bool PaymentItem::FromDictionaryValue(const base::DictionaryValue& value) {
  if (!value.GetString(kPaymentItemLabel, &this->label)) {
    return false;
  }

  const base::DictionaryValue* amount_dict = nullptr;
  if (!value.GetDictionary(kPaymentItemAmount, &amount_dict)) {
    return false;
  }
  if (!this->amount.FromDictionaryValue(*amount_dict)) {
    return false;
  }

  // Pending is optional.
  value.GetBoolean(kPaymentItemPending, &this->pending);

  return true;
}

std::unique_ptr<base::DictionaryValue> PaymentItem::ToDictionaryValue() const {
  std::unique_ptr<base::DictionaryValue> result =
      base::MakeUnique<base::DictionaryValue>();

  result->SetString(kPaymentItemLabel, this->label);
  result->SetDictionary(kPaymentItemAmount, this->amount.ToDictionaryValue());
  result->SetBoolean(kPaymentItemPending, this->pending);

  return result;
}

}  // namespace payments
