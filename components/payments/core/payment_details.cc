// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/payment_details.h"

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "base/values.h"

namespace payments {

namespace {

// These are defined as part of the spec at:
// https://w3c.github.io/payment-request/#payment-details-dictionaries
static const char kPaymentDetailsId[] = "id";
static const char kPaymentDetailsDisplayItems[] = "displayItems";
static const char kPaymentDetailsError[] = "error";
static const char kPaymentDetailsShippingOptions[] = "shippingOptions";
static const char kPaymentDetailsTotal[] = "total";

}  // namespace

PaymentDetails::PaymentDetails() {}
PaymentDetails::~PaymentDetails() = default;

PaymentDetails::PaymentDetails(const PaymentDetails& other) {
  *this = other;
}

PaymentDetails& PaymentDetails::operator=(const PaymentDetails& other) {
  this->id = other.id;
  if (other.total)
    this->total = base::MakeUnique<PaymentItem>(*other.total);
  else
    this->total.reset(nullptr);
  this->display_items = std::vector<PaymentItem>(other.display_items);
  this->shipping_options =
      std::vector<PaymentShippingOption>(other.shipping_options);
  this->modifiers = std::vector<PaymentDetailsModifier>(other.modifiers);
  return *this;
}

bool PaymentDetails::operator==(const PaymentDetails& other) const {
  return this->id == other.id &&
         ((!this->total && !other.total) ||
          (this->total && other.total && *this->total == *other.total)) &&
         this->display_items == other.display_items &&
         this->shipping_options == other.shipping_options &&
         this->modifiers == other.modifiers && this->error == other.error;
}

bool PaymentDetails::operator!=(const PaymentDetails& other) const {
  return !(*this == other);
}

bool PaymentDetails::FromDictionaryValue(const base::DictionaryValue& value,
                                         bool requires_total) {
  this->display_items.clear();
  this->shipping_options.clear();
  this->modifiers.clear();

  // ID is optional.
  value.GetString(kPaymentDetailsId, &this->id);

  const base::DictionaryValue* total_dict = nullptr;
  if (!value.GetDictionary(kPaymentDetailsTotal, &total_dict) &&
      requires_total) {
    return false;
  }
  if (total_dict) {
    this->total = base::MakeUnique<PaymentItem>();
    if (!this->total->FromDictionaryValue(*total_dict))
      return false;
  }

  const base::ListValue* display_items_list = nullptr;
  if (value.GetList(kPaymentDetailsDisplayItems, &display_items_list)) {
    for (size_t i = 0; i < display_items_list->GetSize(); ++i) {
      const base::DictionaryValue* payment_item_dict;
      if (!display_items_list->GetDictionary(i, &payment_item_dict)) {
        return false;
      }
      PaymentItem payment_item;
      if (!payment_item.FromDictionaryValue(*payment_item_dict)) {
        return false;
      }
      this->display_items.push_back(payment_item);
    }
  }

  const base::ListValue* shipping_options_list = nullptr;
  if (value.GetList(kPaymentDetailsShippingOptions, &shipping_options_list)) {
    for (size_t i = 0; i < shipping_options_list->GetSize(); ++i) {
      const base::DictionaryValue* shipping_option_dict;
      if (!shipping_options_list->GetDictionary(i, &shipping_option_dict)) {
        return false;
      }
      PaymentShippingOption shipping_option;
      if (!shipping_option.FromDictionaryValue(*shipping_option_dict)) {
        return false;
      }
      this->shipping_options.push_back(shipping_option);
    }
  }

  // Error is optional.
  value.GetString(kPaymentDetailsError, &this->error);

  return true;
}

}  // namespace payments
