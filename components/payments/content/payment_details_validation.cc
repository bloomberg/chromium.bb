// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_details_validation.h"

#include <set>
#include <vector>

#include "components/payments/content/payments_validators.h"
#include "components/payments/mojom/payment_request.mojom.h"

namespace payments {
namespace {

// Validates ShippingOption or PaymentItem, which happen to have identical
// fields, except for "id", which is present only in ShippingOption.
template <typename T>
bool validateShippingOptionOrPaymentItem(
    const T& item,
    const payments::mojom::PaymentItemPtr& total,
    std::string* error_message) {
  if (!item->amount) {
    *error_message = "Currency amount required";
    return false;
  }

  if (item->amount->currency.empty()) {
    *error_message = "Currency code required";
    return false;
  }

  if (item->amount->value.empty()) {
    *error_message = "Currency value required";
    return false;
  }

  if (item->amount->currency_system.empty()) {
    *error_message = "Currency system can't be empty";
    return false;
  }

  if (!payments::PaymentsValidators::isValidCurrencyCodeFormat(
          item->amount->currency, item->amount->currency_system,
          error_message)) {
    return false;
  }

  if (!payments::PaymentsValidators::isValidAmountFormat(item->amount->value,
                                                         error_message)) {
    return false;
  }
  return true;
}

bool validateDisplayItems(
    const std::vector<payments::mojom::PaymentItemPtr>& items,
    const payments::mojom::PaymentItemPtr& total,
    std::string* error_message) {
  for (const auto& item : items) {
    if (!validateShippingOptionOrPaymentItem(item, total, error_message))
      return false;
  }
  return true;
}

bool validateShippingOptions(
    const std::vector<payments::mojom::PaymentShippingOptionPtr>& options,
    const payments::mojom::PaymentItemPtr& total,
    std::string* error_message) {
  std::set<std::string> uniqueIds;
  for (const auto& option : options) {
    if (option->id.empty()) {
      *error_message = "ShippingOption id required";
      return false;
    }

    if (uniqueIds.find(option->id) != uniqueIds.end()) {
      *error_message = "Duplicate shipping option identifiers are not allowed";
      return false;
    }
    uniqueIds.insert(option->id);

    if (!validateShippingOptionOrPaymentItem(option, total, error_message))
      return false;
  }
  return true;
}

bool validatePaymentDetailsModifiers(
    const std::vector<payments::mojom::PaymentDetailsModifierPtr>& modifiers,
    const payments::mojom::PaymentItemPtr& total,
    std::string* error_message) {
  if (modifiers.empty()) {
    *error_message = "Must specify at least one payment details modifier";
    return false;
  }

  for (const auto& modifier : modifiers) {
    if (!modifier->method_data) {
      *error_message = "Method data required";
      return false;
    }

    if (modifier->method_data->supported_methods.empty()) {
      *error_message = "Must specify at least one payment method identifier";
      return false;
    }

    if (modifier->total) {
      if (!validateShippingOptionOrPaymentItem(modifier->total, total,
                                               error_message))
        return false;

      if (modifier->total->amount->value[0] == '-') {
        *error_message = "Total amount value should be non-negative";
        return false;
      }
    }

    if (modifier->additional_display_items.size()) {
      if (!validateDisplayItems(modifier->additional_display_items, total,
                                error_message)) {
        return false;
      }
    }
  }
  return true;
}

}  // namespace

bool validatePaymentDetails(const mojom::PaymentDetailsPtr& details,
                            std::string* error_message) {
  if (details->total) {
    if (!validateShippingOptionOrPaymentItem(details->total, details->total,
                                             error_message))
      return false;

    if (details->total->amount->value[0] == '-') {
      *error_message = "Total amount value should be non-negative";
      return false;
    }
  }

  if (details->display_items.size()) {
    if (!validateDisplayItems(details->display_items, details->total,
                              error_message))
      return false;
  }

  if (details->shipping_options.size()) {
    if (!validateShippingOptions(details->shipping_options, details->total,
                                 error_message))
      return false;
  }

  if (details->modifiers.size()) {
    if (!validatePaymentDetailsModifiers(details->modifiers, details->total,
                                         error_message))
      return false;
  }
  if (!PaymentsValidators::isValidErrorMsgFormat(details->error, error_message))
    return false;
  return true;
}

}  // namespace payments
