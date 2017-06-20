// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_request_spec.h"

#include <utility>

#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "components/payments/core/payment_method_data.h"
#include "components/payments/core/payment_request_data_util.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace payments {

namespace {

// Returns the card network name associated with a given BasicCardNetwork. Names
// are inspired by https://www.w3.org/Payments/card-network-ids.
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

// Returns the card type associated with the given BasicCardType.
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

}  // namespace

const char kBasicCardMethodName[] = "basic-card";

PaymentRequestSpec::PaymentRequestSpec(
    mojom::PaymentOptionsPtr options,
    mojom::PaymentDetailsPtr details,
    std::vector<mojom::PaymentMethodDataPtr> method_data,
    Observer* observer,
    const std::string& app_locale)
    : options_(std::move(options)),
      details_(std::move(details)),
      app_locale_(app_locale),
      selected_shipping_option_(nullptr) {
  if (observer)
    AddObserver(observer);
  UpdateSelectedShippingOption(/*after_update=*/false);
  PopulateValidatedMethodData(method_data);
}
PaymentRequestSpec::~PaymentRequestSpec() {}

void PaymentRequestSpec::UpdateWith(mojom::PaymentDetailsPtr details) {
  details_ = std::move(details);
  // We reparse the |details_| and update the observers.
  UpdateSelectedShippingOption(/*after_update=*/true);
  NotifyOnSpecUpdated();
  current_update_reason_ = UpdateReason::NONE;
}

void PaymentRequestSpec::AddObserver(Observer* observer) {
  CHECK(observer);
  observers_.AddObserver(observer);
}

void PaymentRequestSpec::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool PaymentRequestSpec::request_shipping() const {
  return options_->request_shipping;
}
bool PaymentRequestSpec::request_payer_name() const {
  return options_->request_payer_name;
}
bool PaymentRequestSpec::request_payer_phone() const {
  return options_->request_payer_phone;
}
bool PaymentRequestSpec::request_payer_email() const {
  return options_->request_payer_email;
}

PaymentShippingType PaymentRequestSpec::shipping_type() const {
  // Transform Mojo-specific enum into platform-agnostic equivalent.
  switch (options_->shipping_type) {
    case mojom::PaymentShippingType::DELIVERY:
      return PaymentShippingType::DELIVERY;
    case payments::mojom::PaymentShippingType::PICKUP:
      return PaymentShippingType::PICKUP;
    case payments::mojom::PaymentShippingType::SHIPPING:
      return PaymentShippingType::SHIPPING;
    default:
      NOTREACHED();
  }
  // Needed for compilation on some platforms.
  return PaymentShippingType::SHIPPING;
}

bool PaymentRequestSpec::IsMethodSupportedThroughBasicCard(
    const std::string& method_name) {
  return basic_card_specified_networks_.count(method_name) > 0;
}

base::string16 PaymentRequestSpec::GetFormattedCurrencyAmount(
    const mojom::PaymentCurrencyAmountPtr& currency_amount) {
  CurrencyFormatter* formatter = GetOrCreateCurrencyFormatter(
      currency_amount->currency, currency_amount->currency_system, app_locale_);
  return formatter->Format(currency_amount->value);
}

std::string PaymentRequestSpec::GetFormattedCurrencyCode(
    const mojom::PaymentCurrencyAmountPtr& currency_amount) {
  CurrencyFormatter* formatter = GetOrCreateCurrencyFormatter(
      currency_amount->currency, currency_amount->currency_system, app_locale_);

  return formatter->formatted_currency_code();
}

void PaymentRequestSpec::StartWaitingForUpdateWith(
    PaymentRequestSpec::UpdateReason reason) {
  current_update_reason_ = reason;
  for (auto& observer : observers_) {
    observer.OnStartUpdating(reason);
  }
}

bool PaymentRequestSpec::IsMixedCurrency() const {
  const std::string& total_currency = details_->total->amount->currency;
  return std::any_of(details_->display_items.begin(),
                     details_->display_items.end(),
                     [&total_currency](const mojom::PaymentItemPtr& item) {
                       return item->amount->currency != total_currency;
                     });
}

void PaymentRequestSpec::PopulateValidatedMethodData(
    const std::vector<mojom::PaymentMethodDataPtr>& method_data_mojom) {
  std::vector<PaymentMethodData> method_data_vector;
  method_data_vector.reserve(method_data_mojom.size());
  for (const mojom::PaymentMethodDataPtr& method_data_entry :
       method_data_mojom) {
    for (const std::string& method : method_data_entry->supported_methods) {
      stringified_method_data_[method].insert(
          method_data_entry->stringified_data);
    }

    PaymentMethodData method_data;
    method_data.supported_methods = method_data_entry->supported_methods;

    // Transfer the supported basic card networks (visa, amex) and types
    // (credit, debit).
    for (const mojom::BasicCardNetwork& network :
         method_data_entry->supported_networks) {
      method_data.supported_networks.push_back(
          GetBasicCardNetworkName(network));
    }
    for (const mojom::BasicCardType& type :
         method_data_entry->supported_types) {
      autofill::CreditCard::CardType card_type = GetBasicCardType(type);
      method_data.supported_types.insert(card_type);
      supported_card_types_set_.insert(card_type);
    }

    method_data_vector.push_back(std::move(method_data));
  }

  // TODO(rouslan): Parse card types (credit, debit, prepaid) in data_util, so
  // iOS can use it as well. http://crbug.com/602665
  data_util::ParseBasicCardSupportedNetworks(method_data_vector,
                                             &supported_card_networks_,
                                             &basic_card_specified_networks_);
  supported_card_networks_set_.insert(supported_card_networks_.begin(),
                                      supported_card_networks_.end());

  // Omitting the card types means all 3 card types are supported.
  if (supported_card_types_set_.empty()) {
    supported_card_types_set_.insert(autofill::CreditCard::CARD_TYPE_CREDIT);
    supported_card_types_set_.insert(autofill::CreditCard::CARD_TYPE_DEBIT);
    supported_card_types_set_.insert(autofill::CreditCard::CARD_TYPE_PREPAID);
  }

  // Let the user decide whether an unknown card type should be used.
  supported_card_types_set_.insert(autofill::CreditCard::CARD_TYPE_UNKNOWN);
}

void PaymentRequestSpec::UpdateSelectedShippingOption(bool after_update) {
  if (!request_shipping())
    return;

  selected_shipping_option_ = nullptr;
  selected_shipping_option_error_.clear();
  if (details().shipping_options.empty()) {
    // No options are provided by the merchant.
    if (after_update) {
      // This is after an update, which means that the selected address is not
      // supported. The merchant may have customized the error string, or a
      // generic one is used.
      if (!details().error.empty()) {
        selected_shipping_option_error_ = base::UTF8ToUTF16(details().error);
      } else {
        // The generic error string depends on the shipping type.
        switch (shipping_type()) {
          case PaymentShippingType::DELIVERY:
            selected_shipping_option_error_ = l10n_util::GetStringUTF16(
                IDS_PAYMENTS_UNSUPPORTED_DELIVERY_ADDRESS);
            break;
          case PaymentShippingType::PICKUP:
            selected_shipping_option_error_ = l10n_util::GetStringUTF16(
                IDS_PAYMENTS_UNSUPPORTED_PICKUP_ADDRESS);
            break;
          case PaymentShippingType::SHIPPING:
            selected_shipping_option_error_ = l10n_util::GetStringUTF16(
                IDS_PAYMENTS_UNSUPPORTED_SHIPPING_ADDRESS);
            break;
        }
      }
    }
    return;
  }

  // As per the spec, the selected shipping option should initially be the last
  // one in the array that has its selected field set to true. If none are
  // selected by the merchant, |selected_shipping_option_| stays nullptr.
  auto selected_shipping_option_it = std::find_if(
      details().shipping_options.rbegin(), details().shipping_options.rend(),
      [](const payments::mojom::PaymentShippingOptionPtr& element) {
        return element->selected;
      });
  if (selected_shipping_option_it != details().shipping_options.rend()) {
    selected_shipping_option_ = selected_shipping_option_it->get();
  }
}

void PaymentRequestSpec::NotifyOnSpecUpdated() {
  for (auto& observer : observers_)
    observer.OnSpecUpdated();
}

CurrencyFormatter* PaymentRequestSpec::GetOrCreateCurrencyFormatter(
    const std::string& currency_code,
    const std::string& currency_system,
    const std::string& locale_name) {
  // Create a currency formatter for |currency_code|, or if already created
  // return the cached version.
  std::pair<std::map<std::string, CurrencyFormatter>::iterator, bool>
      emplace_result = currency_formatters_.emplace(
          std::piecewise_construct, std::forward_as_tuple(currency_code),
          std::forward_as_tuple(currency_code, currency_system, locale_name));

  return &(emplace_result.first->second);
}

}  // namespace payments
