// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_request_spec.h"

#include <utility>

#include "base/feature_list.h"
#include "base/logging.h"
#include "base/strings/utf_string_conversions.h"
#include "components/payments/core/features.h"
#include "components/payments/core/payment_instrument.h"
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

PaymentMethodData CreatePaymentMethodData(
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

// Validates the |method_data| and fills |supported_card_networks_|,
// |supported_card_networks_set_|, |basic_card_specified_networks_|,
// and |url_payment_method_identifiers_|.
void PopulateValidatedMethodData(
    const std::vector<PaymentMethodData>& method_data_vector,
    std::vector<std::string>* supported_card_networks,
    std::set<std::string>* basic_card_specified_networks,
    std::set<std::string>* supported_card_networks_set,
    std::set<autofill::CreditCard::CardType>* supported_card_types_set,
    std::vector<std::string>* url_payment_method_identifiers,
    std::map<std::string, std::set<std::string>>* stringified_method_data) {
  data_util::ParseSupportedMethods(method_data_vector, supported_card_networks,
                                   basic_card_specified_networks,
                                   url_payment_method_identifiers);
  supported_card_networks_set->insert(supported_card_networks->begin(),
                                      supported_card_networks->end());

  data_util::ParseSupportedCardTypes(method_data_vector,
                                     supported_card_types_set);
}

void PopulateValidatedMethodData(
    const std::vector<mojom::PaymentMethodDataPtr>& method_data_mojom,
    std::vector<std::string>* supported_card_networks,
    std::set<std::string>* basic_card_specified_networks,
    std::set<std::string>* supported_card_networks_set,
    std::set<autofill::CreditCard::CardType>* supported_card_types_set,
    std::vector<std::string>* url_payment_method_identifiers,
    std::map<std::string, std::set<std::string>>* stringified_method_data) {
  std::vector<PaymentMethodData> method_data_vector;
  method_data_vector.reserve(method_data_mojom.size());
  for (const mojom::PaymentMethodDataPtr& method_data_entry :
       method_data_mojom) {
    for (const std::string& method : method_data_entry->supported_methods) {
      (*stringified_method_data)[method].insert(
          method_data_entry->stringified_data);
    }

    method_data_vector.push_back(CreatePaymentMethodData(method_data_entry));
  }

  PopulateValidatedMethodData(
      method_data_vector, supported_card_networks,
      basic_card_specified_networks, supported_card_networks_set,
      supported_card_types_set, url_payment_method_identifiers,
      stringified_method_data);
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
  PopulateValidatedMethodData(
      method_data, &supported_card_networks_, &basic_card_specified_networks_,
      &supported_card_networks_set_, &supported_card_types_set_,
      &url_payment_method_identifiers_, &stringified_method_data_);
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

const mojom::PaymentItemPtr& PaymentRequestSpec::GetTotal(
    PaymentInstrument* selected_instrument) const {
  const mojom::PaymentDetailsModifierPtr* modifier =
      GetApplicableModifier(selected_instrument);
  return modifier ? (*modifier)->total : details().total;
}

std::vector<const mojom::PaymentItemPtr*> PaymentRequestSpec::GetDisplayItems(
    PaymentInstrument* selected_instrument) const {
  std::vector<const mojom::PaymentItemPtr*> display_items;
  const mojom::PaymentDetailsModifierPtr* modifier =
      GetApplicableModifier(selected_instrument);
  for (const auto& item : details().display_items) {
    display_items.push_back(&item);
  }

  if (modifier) {
    for (const auto& additional_item : (*modifier)->additional_display_items) {
      display_items.push_back(&additional_item);
    }
  }
  return display_items;
}

const std::vector<mojom::PaymentShippingOptionPtr>&
PaymentRequestSpec::GetShippingOptions() const {
  return details().shipping_options;
}

const mojom::PaymentDetailsModifierPtr*
PaymentRequestSpec::GetApplicableModifier(
    PaymentInstrument* selected_instrument) const {
  if (!selected_instrument ||
      !base::FeatureList::IsEnabled(features::kWebPaymentsModifiers))
    return nullptr;

  for (const auto& modifier : details().modifiers) {
    std::vector<std::string> supported_networks;
    std::set<autofill::CreditCard::CardType> supported_types;
    // The following 4 are unused but required by PopulateValidatedMethodData.
    std::set<std::string> basic_card_specified_networks;
    std::set<std::string> supported_card_networks_set;
    std::vector<std::string> url_payment_method_identifiers;
    std::map<std::string, std::set<std::string>> stringified_method_data;
    PopulateValidatedMethodData(
        {CreatePaymentMethodData(modifier->method_data)}, &supported_networks,
        &basic_card_specified_networks, &supported_card_networks_set,
        &supported_types, &url_payment_method_identifiers,
        &stringified_method_data);

    if (selected_instrument->IsValidForModifier(
            modifier->method_data->supported_methods, supported_types,
            supported_networks)) {
      return &modifier;
    }
  }
  return nullptr;
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
