// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_request_spec.h"

#include <utility>

#include "base/logging.h"

namespace payments {

const char kBasicCardMethodName[] = "basic-card";

PaymentRequestSpec::PaymentRequestSpec(
    mojom::PaymentOptionsPtr options,
    mojom::PaymentDetailsPtr details,
    std::vector<mojom::PaymentMethodDataPtr> method_data,
    Observer* observer,
    const std::string& app_locale)
    : options_(std::move(options)),
      details_(std::move(details)),
      app_locale_(app_locale) {
  if (observer)
    AddObserver(observer);
  PopulateValidatedMethodData(method_data);
}
PaymentRequestSpec::~PaymentRequestSpec() {}

void PaymentRequestSpec::AddObserver(Observer* observer) {
  CHECK(observer);
  observers_.AddObserver(observer);
}

void PaymentRequestSpec::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
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

void PaymentRequestSpec::PopulateValidatedMethodData(
    const std::vector<mojom::PaymentMethodDataPtr>& method_data) {
  if (method_data.empty()) {
    LOG(ERROR) << "Invalid payment methods or data";
    NotifyOnInvalidSpecProvided();
    return;
  }

  std::set<std::string> card_networks{"amex",     "diners",     "discover",
                                      "jcb",      "mastercard", "mir",
                                      "unionpay", "visa"};
  for (const mojom::PaymentMethodDataPtr& method_data_entry : method_data) {
    std::vector<std::string> supported_methods =
        method_data_entry->supported_methods;
    if (supported_methods.empty()) {
      LOG(ERROR) << "Invalid payment methods or data";
      NotifyOnInvalidSpecProvided();
      return;
    }

    for (const std::string& method : supported_methods) {
      if (method.empty())
        continue;

      // If a card network is specified right in "supportedMethods", add it.
      auto card_it = card_networks.find(method);
      if (card_it != card_networks.end()) {
        supported_card_networks_.push_back(method);
        // |method| removed from |card_networks| so that it is not doubly added
        // to |supported_card_networks_| if "basic-card" is specified with no
        // supported networks.
        card_networks.erase(card_it);
      } else if (method == kBasicCardMethodName) {
        // For the "basic-card" method, check "supportedNetworks".
        if (method_data_entry->supported_networks.empty()) {
          // Empty |supported_networks| means all networks are supported.
          supported_card_networks_.insert(supported_card_networks_.end(),
                                          card_networks.begin(),
                                          card_networks.end());
          basic_card_specified_networks_.insert(card_networks.begin(),
                                                card_networks.end());
          // Clear the set so that no further networks are added to
          // |supported_card_networks_|.
          card_networks.clear();
        } else {
          // The merchant has specified a few basic card supported networks. Use
          // the mapping to transform to known basic-card types.
          using mojom::BasicCardNetwork;
          std::unordered_map<BasicCardNetwork, std::string> networks = {
              {BasicCardNetwork::AMEX, "amex"},
              {BasicCardNetwork::DINERS, "diners"},
              {BasicCardNetwork::DISCOVER, "discover"},
              {BasicCardNetwork::JCB, "jcb"},
              {BasicCardNetwork::MASTERCARD, "mastercard"},
              {BasicCardNetwork::MIR, "mir"},
              {BasicCardNetwork::UNIONPAY, "unionpay"},
              {BasicCardNetwork::VISA, "visa"}};
          for (const BasicCardNetwork& supported_network :
               method_data_entry->supported_networks) {
            // Make sure that the network was not already added to
            // |supported_card_networks_|.
            auto card_it = card_networks.find(networks[supported_network]);
            if (card_it != card_networks.end()) {
              supported_card_networks_.push_back(networks[supported_network]);
              basic_card_specified_networks_.insert(
                  networks[supported_network]);
              card_networks.erase(card_it);
            }
          }
        }
      }
    }
  }

  supported_card_networks_set_.insert(supported_card_networks_.begin(),
                                      supported_card_networks_.end());
}

void PaymentRequestSpec::NotifyOnInvalidSpecProvided() {
  for (auto& observer : observers_)
    observer.OnInvalidSpecProvided();
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
