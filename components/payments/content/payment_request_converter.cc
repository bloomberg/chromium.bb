// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_request_converter.h"

#include <memory>

#include "base/memory/ptr_util.h"
#include "components/payments/core/payment_currency_amount.h"
#include "components/payments/core/payment_details.h"
#include "components/payments/core/payment_details_modifier.h"
#include "components/payments/core/payment_item.h"
#include "components/payments/core/payment_method_data.h"
#include "components/payments/core/payment_shipping_option.h"

namespace payments {
namespace {

PaymentCurrencyAmount ConvertPaymentCurrencyAmount(
    const mojom::PaymentCurrencyAmountPtr& amount_entry) {
  PaymentCurrencyAmount amount;
  amount.currency = amount_entry->currency;
  amount.value = amount_entry->value;
  amount.currency_system = amount_entry->currency_system;
  return amount;
}

PaymentItem ConvertPaymentItem(const mojom::PaymentItemPtr& item_entry) {
  PaymentItem item;
  item.label = item_entry->label;
  if (item_entry->amount)
    item.amount = ConvertPaymentCurrencyAmount(item_entry->amount);
  item.pending = item_entry->pending;
  return item;
}

PaymentDetailsModifier ConvertPaymentDetailsModifier(
    const mojom::PaymentDetailsModifierPtr& modifier_entry) {
  PaymentDetailsModifier modifier;
  if (modifier_entry->total) {
    modifier.total = base::MakeUnique<PaymentItem>(
        ConvertPaymentItem(modifier_entry->total));
  }
  modifier.additional_display_items.reserve(
      modifier_entry->additional_display_items.size());
  for (const mojom::PaymentItemPtr& additional_display_item :
       modifier_entry->additional_display_items) {
    modifier.additional_display_items.push_back(
        ConvertPaymentItem(additional_display_item));
  }
  if (modifier_entry->method_data)
    modifier.method_data =
        ConvertPaymentMethodData(modifier_entry->method_data);
  return modifier;
}

PaymentShippingOption ConvertPaymentShippingOption(
    const mojom::PaymentShippingOptionPtr& option_entry) {
  PaymentShippingOption option;
  option.id = option_entry->id;
  option.label = option_entry->label;
  if (option_entry->amount)
    option.amount = ConvertPaymentCurrencyAmount(option_entry->amount);
  option.selected = option_entry->selected;
  return option;
}

}  // namespace

autofill::CreditCard::CardType GetBasicCardType(
    const mojom::BasicCardType& type) {
  switch (type) {
    case mojom::BasicCardType::CREDIT:
      return autofill::CreditCard::CARD_TYPE_CREDIT;
    case mojom::BasicCardType::DEBIT:
      return autofill::CreditCard::CARD_TYPE_DEBIT;
    case mojom::BasicCardType::PREPAID:
      return autofill::CreditCard::CARD_TYPE_PREPAID;
  }
  NOTREACHED();
  return autofill::CreditCard::CARD_TYPE_UNKNOWN;
}

std::string GetBasicCardNetworkName(const mojom::BasicCardNetwork& network) {
  switch (network) {
    case mojom::BasicCardNetwork::AMEX:
      return "amex";
    case mojom::BasicCardNetwork::DINERS:
      return "diners";
    case mojom::BasicCardNetwork::DISCOVER:
      return "discover";
    case mojom::BasicCardNetwork::JCB:
      return "jcb";
    case mojom::BasicCardNetwork::MASTERCARD:
      return "mastercard";
    case mojom::BasicCardNetwork::MIR:
      return "mir";
    case mojom::BasicCardNetwork::UNIONPAY:
      return "unionpay";
    case mojom::BasicCardNetwork::VISA:
      return "visa";
  }
  NOTREACHED();
  return std::string();
}

PaymentMethodData ConvertPaymentMethodData(
    const mojom::PaymentMethodDataPtr& method_data_entry) {
  PaymentMethodData method_data;
  method_data.supported_methods = method_data_entry->supported_methods;

  // Transfer the supported basic card networks (visa, amex) and types
  // (credit, debit).
  for (const mojom::BasicCardNetwork& network :
       method_data_entry->supported_networks) {
    method_data.supported_networks.push_back(GetBasicCardNetworkName(network));
  }
  for (const mojom::BasicCardType& type : method_data_entry->supported_types) {
    autofill::CreditCard::CardType card_type = GetBasicCardType(type);
    method_data.supported_types.insert(card_type);
  }
  return method_data;
}

PaymentDetails ConvertPaymentDetails(
    const mojom::PaymentDetailsPtr& details_entry) {
  PaymentDetails details;
  if (details_entry->total) {
    details.total =
        base::MakeUnique<PaymentItem>(ConvertPaymentItem(details_entry->total));
  }
  details.display_items.reserve(details_entry->display_items.size());
  for (const mojom::PaymentItemPtr& display_item :
       details_entry->display_items) {
    details.display_items.push_back(ConvertPaymentItem(display_item));
  }
  details.shipping_options.reserve(details_entry->shipping_options.size());
  for (const mojom::PaymentShippingOptionPtr& shipping_option :
       details_entry->shipping_options) {
    details.shipping_options.push_back(
        ConvertPaymentShippingOption(shipping_option));
  }
  details.modifiers.reserve(details_entry->modifiers.size());
  for (const mojom::PaymentDetailsModifierPtr& modifier :
       details_entry->modifiers) {
    details.modifiers.push_back(ConvertPaymentDetailsModifier(modifier));
  }
  details.error = details_entry->error;
  if (details_entry->id.has_value())
    details.id = details_entry->id.value();
  return details;
}

}  // namespace payments
