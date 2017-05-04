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
      selected_shipping_option_(nullptr),
      observer_for_testing_(nullptr) {
  if (observer)
    AddObserver(observer);
  UpdateSelectedShippingOption();
  PopulateValidatedMethodData(method_data);
}
PaymentRequestSpec::~PaymentRequestSpec() {}

void PaymentRequestSpec::UpdateWith(mojom::PaymentDetailsPtr details) {
  details_ = std::move(details);
  // We reparse the |details_| and update the observers.
  UpdateSelectedShippingOption();
  NotifyOnSpecUpdated();
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
    const std::string& amount) {
  CurrencyFormatter* formatter = GetOrCreateCurrencyFormatter(
      details_->total->amount->currency,
      details_->total->amount->currency_system, app_locale_);
  return formatter->Format(amount);
}

std::string PaymentRequestSpec::GetFormattedCurrencyCode() {
  CurrencyFormatter* formatter = GetOrCreateCurrencyFormatter(
      details_->total->amount->currency,
      details_->total->amount->currency_system, app_locale_);

  return formatter->formatted_currency_code();
}

void PaymentRequestSpec::StartWaitingForUpdateWith(
    PaymentRequestSpec::UpdateReason reason) {
  for (auto& observer : observers_) {
    observer.OnStartUpdating(reason);
  }
}

void PaymentRequestSpec::PopulateValidatedMethodData(
    const std::vector<mojom::PaymentMethodDataPtr>& method_data_mojom) {
  std::vector<PaymentMethodData> method_data_vector;
  method_data_vector.reserve(method_data_mojom.size());
  for (const mojom::PaymentMethodDataPtr& method_data_entry :
       method_data_mojom) {
    PaymentMethodData method_data;
    method_data.supported_methods = method_data_entry->supported_methods;
    // Transfer the supported basic card networks.
    std::vector<std::string> supported_networks;
    for (const mojom::BasicCardNetwork& network :
         method_data_entry->supported_networks) {
      supported_networks.push_back(GetBasicCardNetworkName(network));
    }
    method_data.supported_networks = std::move(supported_networks);

    // TODO(crbug.com/708603): Add browser-side support for
    // |method_data.supported_types|.
    method_data_vector.push_back(std::move(method_data));
  }

  data_util::ParseBasicCardSupportedNetworks(method_data_vector,
                                             &supported_card_networks_,
                                             &basic_card_specified_networks_);
  supported_card_networks_set_.insert(supported_card_networks_.begin(),
                                      supported_card_networks_.end());
}

void PaymentRequestSpec::UpdateSelectedShippingOption() {
  if (!request_shipping())
    return;

  selected_shipping_option_error_.clear();
  // As per the spec, the selected shipping option should initially be the last
  // one in the array that has its selected field set to true.
  auto selected_shipping_option_it = std::find_if(
      details().shipping_options.rbegin(), details().shipping_options.rend(),
      [](const payments::mojom::PaymentShippingOptionPtr& element) {
        return element->selected;
      });
  if (selected_shipping_option_it != details().shipping_options.rend()) {
    selected_shipping_option_ = selected_shipping_option_it->get();
  } else {
    // It's possible that there is no selected shipping option.
    if (!details().error.empty()) {
      selected_shipping_option_error_ = base::UTF8ToUTF16(details().error);
    } else {
      selected_shipping_option_error_ =
          l10n_util::GetStringUTF16(IDS_PAYMENTS_UNSUPPORTED_SHIPPING_ADDRESS);
    }
    selected_shipping_option_ = nullptr;
  }
}

void PaymentRequestSpec::NotifyOnSpecUpdated() {
  for (auto& observer : observers_)
    observer.OnSpecUpdated();
  if (observer_for_testing_)
    observer_for_testing_->OnSpecUpdated();
}

CurrencyFormatter* PaymentRequestSpec::GetOrCreateCurrencyFormatter(
    const std::string& currency_code,
    const std::string& currency_system,
    const std::string& locale_name) {
  if (!currency_formatter_) {
    currency_formatter_.reset(
        new CurrencyFormatter(currency_code, currency_system, locale_name));
  }
  return currency_formatter_.get();
}

}  // namespace payments
