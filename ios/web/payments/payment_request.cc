// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/payments/payment_request.h"

#include "base/strings/utf_string_conversions.h"
#include "base/values.h"

namespace {

// All of these are defined here (even though most are only used once each) so
// the format details are easy to locate and update or compare to the spec doc.
// (https://w3c.github.io/browser-payment-api/).
static const char kAddressAddressLine[] = "addressLine";
static const char kAddressCity[] = "city";
static const char kAddressCountry[] = "country";
static const char kAddressDependentLocality[] = "dependentLocality";
static const char kAddressLanguageCode[] = "languageCode";
static const char kAddressOrganization[] = "organization";
static const char kAddressPhone[] = "phone";
static const char kAddressPostalCode[] = "postalCode";
static const char kAddressRecipient[] = "recipient";
static const char kAddressRegion[] = "region";
static const char kAddressSortingCode[] = "sortingCode";
static const char kCardBillingAddress[] = "billingAddress";
static const char kCardCardholderName[] = "cardholderName";
static const char kCardCardNumber[] = "cardNumber";
static const char kCardCardSecurityCode[] = "cardSecurityCode";
static const char kCardExpiryMonth[] = "expiryMonth";
static const char kCardExpiryYear[] = "expiryYear";
static const char kMethodDataData[] = "data";
static const char kPaymentCurrencyAmountCurrencySystemISO4217[] =
    "urn:iso:std:iso:4217";
static const char kPaymentCurrencyAmountCurrencySystem[] = "currencySystem";
static const char kPaymentCurrencyAmountCurrency[] = "currency";
static const char kPaymentCurrencyAmountValue[] = "value";
static const char kPaymentDetailsDisplayItems[] = "displayItems";
static const char kPaymentDetailsError[] = "error";
static const char kPaymentDetailsShippingOptions[] = "shippingOptions";
static const char kPaymentDetailsTotal[] = "total";
static const char kPaymentItemAmount[] = "amount";
static const char kPaymentItemLabel[] = "label";
static const char kPaymentItemPending[] = "pending";
static const char kPaymentOptionsRequestPayerEmail[] = "requestPayerEmail";
static const char kPaymentOptionsRequestPayerName[] = "requestPayerName";
static const char kPaymentOptionsRequestPayerPhone[] = "requestPayerPhone";
static const char kPaymentOptionsRequestShipping[] = "requestShipping";
static const char kPaymentOptionsShippingTypeDelivery[] = "delivery";
static const char kPaymentOptionsShippingTypePickup[] = "pickup";
static const char kPaymentOptionsShippingType[] = "shippingType";
static const char kPaymentRequestDetails[] = "details";
static const char kPaymentRequestId[] = "paymentRequestID";
static const char kPaymentRequestMethodData[] = "methodData";
static const char kPaymentRequestOptions[] = "options";
static const char kPaymentResponseDetails[] = "details";
static const char kPaymentResponseMethodName[] = "methodName";
static const char kPaymentResponsePayerEmail[] = "payerEmail";
static const char kPaymentResponsePayerName[] = "payerName";
static const char kPaymentResponsePayerPhone[] = "payerPhone";
static const char kPaymentResponseShippingAddress[] = "shippingAddress";
static const char kPaymentResponseShippingOption[] = "shippingOption";
static const char kPaymentShippingOptionAmount[] = "amount";
static const char kPaymentShippingOptionId[] = "id";
static const char kPaymentShippingOptionLabel[] = "label";
static const char kPaymentShippingOptionSelected[] = "selected";
static const char kSupportedMethods[] = "supportedMethods";

}  // namespace

namespace web {

PaymentAddress::PaymentAddress() {}
PaymentAddress::PaymentAddress(const PaymentAddress& other) = default;
PaymentAddress::~PaymentAddress() = default;

bool PaymentAddress::operator==(const PaymentAddress& other) const {
  return this->country == other.country &&
         this->address_line == other.address_line &&
         this->region == other.region && this->city == other.city &&
         this->dependent_locality == other.dependent_locality &&
         this->postal_code == other.postal_code &&
         this->sorting_code == other.sorting_code &&
         this->language_code == other.language_code &&
         this->organization == other.organization &&
         this->recipient == other.recipient && this->care_of == other.care_of &&
         this->phone == other.phone;
}

bool PaymentAddress::operator!=(const PaymentAddress& other) const {
  return !(*this == other);
}

std::unique_ptr<base::DictionaryValue> PaymentAddress::ToDictionaryValue()
    const {
  std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue());

  if (!this->country.empty())
    result->SetString(kAddressCountry, this->country);

  if (!this->address_line.empty()) {
    std::unique_ptr<base::ListValue> address_line(new base::ListValue);
    for (base::string16 address_line_string : this->address_line) {
      if (!address_line_string.empty())
        address_line->AppendString(address_line_string);
    }
    result->Set(kAddressAddressLine, std::move(address_line));
  }

  if (!this->region.empty())
    result->SetString(kAddressRegion, this->region);
  if (!this->city.empty())
    result->SetString(kAddressCity, this->city);
  if (!this->dependent_locality.empty())
    result->SetString(kAddressDependentLocality, this->dependent_locality);
  if (!this->postal_code.empty())
    result->SetString(kAddressPostalCode, this->postal_code);
  if (!this->sorting_code.empty())
    result->SetString(kAddressSortingCode, this->sorting_code);
  if (!this->language_code.empty())
    result->SetString(kAddressLanguageCode, this->language_code);
  if (!this->organization.empty())
    result->SetString(kAddressOrganization, this->organization);
  if (!this->recipient.empty())
    result->SetString(kAddressRecipient, this->recipient);
  if (!this->phone.empty())
    result->SetString(kAddressPhone, this->phone);

  return result;
}

PaymentMethodData::PaymentMethodData() {}
PaymentMethodData::PaymentMethodData(const PaymentMethodData& other) = default;
PaymentMethodData::~PaymentMethodData() = default;

bool PaymentMethodData::operator==(const PaymentMethodData& other) const {
  return this->supported_methods == other.supported_methods &&
         this->data == other.data;
}

bool PaymentMethodData::operator!=(const PaymentMethodData& other) const {
  return !(*this == other);
}

bool PaymentMethodData::FromDictionaryValue(
    const base::DictionaryValue& value) {
  this->supported_methods.clear();

  const base::ListValue* supported_methods_list = nullptr;
  // At least one supported method is required.
  if (!value.GetList(kSupportedMethods, &supported_methods_list) ||
      supported_methods_list->GetSize() == 0) {
    return false;
  }
  for (size_t i = 0; i < supported_methods_list->GetSize(); ++i) {
    base::string16 supported_method;
    if (!supported_methods_list->GetString(i, &supported_method)) {
      return false;
    }
    this->supported_methods.push_back(supported_method);
  }

  // Data is optional.
  value.GetString(kMethodDataData, &this->data);

  return true;
}

PaymentCurrencyAmount::PaymentCurrencyAmount()
    // By default, the currency is defined by [ISO4217]. For example, USD for
    // US Dollars.
    : currency_system(
          base::ASCIIToUTF16(kPaymentCurrencyAmountCurrencySystemISO4217)) {}

PaymentCurrencyAmount::~PaymentCurrencyAmount() = default;

bool PaymentCurrencyAmount::operator==(
    const PaymentCurrencyAmount& other) const {
  return this->currency == other.currency && this->value == other.value;
}

bool PaymentCurrencyAmount::operator!=(
    const PaymentCurrencyAmount& other) const {
  return !(*this == other);
}

bool PaymentCurrencyAmount::FromDictionaryValue(
    const base::DictionaryValue& value) {
  if (!value.GetString(kPaymentCurrencyAmountCurrency, &this->currency)) {
    return false;
  }

  if (!value.GetString(kPaymentCurrencyAmountValue, &this->value)) {
    return false;
  }

  // Currency_system is optional
  value.GetString(kPaymentCurrencyAmountCurrencySystem, &this->currency_system);

  return true;
}

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

PaymentShippingOption::PaymentShippingOption() : selected(false) {}
PaymentShippingOption::PaymentShippingOption(
    const PaymentShippingOption& other) = default;
PaymentShippingOption::~PaymentShippingOption() = default;

bool PaymentShippingOption::operator==(
    const PaymentShippingOption& other) const {
  return this->id == other.id && this->label == other.label &&
         this->amount == other.amount && this->selected == other.selected;
}

bool PaymentShippingOption::operator!=(
    const PaymentShippingOption& other) const {
  return !(*this == other);
}

bool PaymentShippingOption::FromDictionaryValue(
    const base::DictionaryValue& value) {
  if (!value.GetString(kPaymentShippingOptionId, &this->id)) {
    return false;
  }

  if (!value.GetString(kPaymentShippingOptionLabel, &this->label)) {
    return false;
  }

  const base::DictionaryValue* amount_dict = nullptr;
  if (!value.GetDictionary(kPaymentShippingOptionAmount, &amount_dict)) {
    return false;
  }
  if (!this->amount.FromDictionaryValue(*amount_dict)) {
    return false;
  }

  // Selected is optional.
  value.GetBoolean(kPaymentShippingOptionSelected, &this->selected);

  return true;
}

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

PaymentDetails::PaymentDetails() {}
PaymentDetails::PaymentDetails(const PaymentDetails& other) = default;
PaymentDetails::~PaymentDetails() = default;

bool PaymentDetails::operator==(const PaymentDetails& other) const {
  return this->total == other.total &&
         this->display_items == other.display_items &&
         this->shipping_options == other.shipping_options &&
         this->modifiers == other.modifiers && this->error == other.error;
}

bool PaymentDetails::operator!=(const PaymentDetails& other) const {
  return !(*this == other);
}

bool PaymentDetails::FromDictionaryValue(const base::DictionaryValue& value) {
  this->display_items.clear();
  this->shipping_options.clear();
  this->modifiers.clear();

  const base::DictionaryValue* total_dict = nullptr;
  if (!value.GetDictionary(kPaymentDetailsTotal, &total_dict)) {
    return false;
  }
  if (!this->total.FromDictionaryValue(*total_dict)) {
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

PaymentOptions::PaymentOptions()
    : request_payer_name(false),
      request_payer_email(false),
      request_payer_phone(false),
      request_shipping(false),
      shipping_type(PaymentShippingType::SHIPPING) {}
PaymentOptions::~PaymentOptions() = default;

bool PaymentOptions::operator==(const PaymentOptions& other) const {
  return this->request_payer_name == other.request_payer_name &&
         this->request_payer_email == other.request_payer_email &&
         this->request_payer_phone == other.request_payer_phone &&
         this->request_shipping == other.request_shipping &&
         this->shipping_type == other.shipping_type;
}

bool PaymentOptions::operator!=(const PaymentOptions& other) const {
  return !(*this == other);
}

bool PaymentOptions::FromDictionaryValue(const base::DictionaryValue& value) {
  value.GetBoolean(kPaymentOptionsRequestPayerName, &this->request_payer_name);

  value.GetBoolean(kPaymentOptionsRequestPayerEmail,
                   &this->request_payer_email);

  value.GetBoolean(kPaymentOptionsRequestPayerPhone,
                   &this->request_payer_phone);

  value.GetBoolean(kPaymentOptionsRequestShipping, &this->request_shipping);

  base::string16 shipping_type;
  value.GetString(kPaymentOptionsShippingType, &shipping_type);
  if (shipping_type ==
      base::ASCIIToUTF16(kPaymentOptionsShippingTypeDelivery)) {
    this->shipping_type = PaymentShippingType::DELIVERY;
  } else if (shipping_type ==
             base::ASCIIToUTF16(kPaymentOptionsShippingTypePickup)) {
    this->shipping_type = PaymentShippingType::PICKUP;
  } else {
    this->shipping_type = PaymentShippingType::SHIPPING;
  }

  return true;
}

PaymentRequest::PaymentRequest() {}
PaymentRequest::PaymentRequest(const PaymentRequest& other) = default;
PaymentRequest::~PaymentRequest() = default;

bool PaymentRequest::operator==(const PaymentRequest& other) const {
  return this->payment_request_id == other.payment_request_id &&
         this->shipping_address == other.shipping_address &&
         this->shipping_option == other.shipping_option &&
         this->method_data == other.method_data &&
         this->details == other.details && this->options == other.options;
}

bool PaymentRequest::operator!=(const PaymentRequest& other) const {
  return !(*this == other);
}

bool PaymentRequest::FromDictionaryValue(const base::DictionaryValue& value) {
  this->method_data.clear();

  // Parse the payment method data.
  const base::ListValue* method_data_list = nullptr;
  // At least one method is required.
  if (!value.GetList(kPaymentRequestMethodData, &method_data_list) ||
      method_data_list->GetSize() == 0) {
    return false;
  }
  for (size_t i = 0; i < method_data_list->GetSize(); ++i) {
    const base::DictionaryValue* method_data_dict;
    if (!method_data_list->GetDictionary(i, &method_data_dict))
      return false;

    PaymentMethodData method_data;
    if (!method_data.FromDictionaryValue(*method_data_dict))
      return false;
    this->method_data.push_back(method_data);
  }

  // Parse the payment details.
  const base::DictionaryValue* payment_details_dict = nullptr;
  if (!value.GetDictionary(kPaymentRequestDetails, &payment_details_dict) ||
      !this->details.FromDictionaryValue(*payment_details_dict)) {
    return false;
  }

  // Parse the payment options.
  const base::DictionaryValue* payment_options = nullptr;
  // Options field is optional.
  if (value.GetDictionary(kPaymentRequestOptions, &payment_options))
    if (!this->options.FromDictionaryValue(*payment_options))
      return false;

  return true;
}

BasicCardResponse::BasicCardResponse() {}
BasicCardResponse::BasicCardResponse(const BasicCardResponse& other) = default;
BasicCardResponse::~BasicCardResponse() = default;

bool BasicCardResponse::operator==(const BasicCardResponse& other) const {
  return this->cardholder_name == other.cardholder_name &&
         this->card_number == other.card_number &&
         this->expiry_month == other.expiry_month &&
         this->expiry_year == other.expiry_year &&
         this->card_security_code == other.card_security_code &&
         this->billing_address == other.billing_address;
}

bool BasicCardResponse::operator!=(const BasicCardResponse& other) const {
  return !(*this == other);
}

std::unique_ptr<base::DictionaryValue> BasicCardResponse::ToDictionaryValue()
    const {
  std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue());

  if (!this->cardholder_name.empty())
    result->SetString(kCardCardholderName, this->cardholder_name);
  result->SetString(kCardCardNumber, this->card_number);
  if (!this->expiry_month.empty())
    result->SetString(kCardExpiryMonth, this->expiry_month);
  if (!this->expiry_year.empty())
    result->SetString(kCardExpiryYear, this->expiry_year);
  if (!this->card_security_code.empty())
    result->SetString(kCardCardSecurityCode, this->card_security_code);
  if (!this->billing_address.ToDictionaryValue()->empty())
    result->Set(kCardBillingAddress, this->billing_address.ToDictionaryValue());

  return result;
}

PaymentResponse::PaymentResponse() {}
PaymentResponse::PaymentResponse(const PaymentResponse& other) = default;
PaymentResponse::~PaymentResponse() = default;

bool PaymentResponse::operator==(const PaymentResponse& other) const {
  return this->payment_request_id == other.payment_request_id &&
         this->method_name == other.method_name &&
         this->details == other.details &&
         this->shipping_address == other.shipping_address &&
         this->shipping_option == other.shipping_option &&
         this->payer_name == other.payer_name &&
         this->payer_email == other.payer_email &&
         this->payer_phone == other.payer_phone;
}

bool PaymentResponse::operator!=(const PaymentResponse& other) const {
  return !(*this == other);
}

std::unique_ptr<base::DictionaryValue> PaymentResponse::ToDictionaryValue()
    const {
  std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue());

  result->SetString(kPaymentRequestId, this->payment_request_id);
  result->SetString(kPaymentResponseMethodName, this->method_name);
  result->Set(kPaymentResponseDetails, this->details.ToDictionaryValue());
  if (!this->shipping_address.ToDictionaryValue()->empty()) {
    result->Set(kPaymentResponseShippingAddress,
                this->shipping_address.ToDictionaryValue());
  }
  if (!this->shipping_option.empty())
    result->SetString(kPaymentResponseShippingOption, this->shipping_option);
  if (!this->payer_name.empty())
    result->SetString(kPaymentResponsePayerName, this->payer_name);
  if (!this->payer_email.empty())
    result->SetString(kPaymentResponsePayerEmail, this->payer_email);
  if (!this->payer_phone.empty())
    result->SetString(kPaymentResponsePayerPhone, this->payer_phone);

  return result;
}

}  // namespace web
