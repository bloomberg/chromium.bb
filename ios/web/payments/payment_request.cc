// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/payments/payment_request.h"

#include "base/values.h"

namespace {

// All of these are defined here (even though most are only used once each) so
// the format details are easy to locate and update or compare to the spec doc.
// (https://w3c.github.io/browser-payment-api/).
static const char kAddressCountry[] = "country";
static const char kAddressAddressLine[] = "addressLine";
static const char kAddressRegion[] = "region";
static const char kAddressCity[] = "city";
static const char kAddressDependentLocality[] = "dependentLocality";
static const char kAddressPostalCode[] = "postalCode";
static const char kAddressSortingCode[] = "sortingCode";
static const char kAddressLanguageCode[] = "languageCode";
static const char kAddressOrganization[] = "organization";
static const char kAddressRecipient[] = "recipient";
static const char kAddressPhone[] = "phone";
static const char kMethodData[] = "methodData";
static const char kSupportedMethods[] = "supportedMethods";
static const char kData[] = "data";
static const char kPaymentDetails[] = "details";
static const char kPaymentDetailsTotal[] = "total";
static const char kPaymentDetailsTotalAmount[] = "amount";
static const char kPaymentDetailsTotalAmountCurrency[] = "currency";
static const char kPaymentDetailsTotalAmountValue[] = "value";
static const char kMethodName[] = "methodName";
static const char kCardCardholderName[] = "cardholderName";
static const char kCardCardNumber[] = "cardNumber";
static const char kCardExpiryMonth[] = "expiryMonth";
static const char kCardExpiryYear[] = "expiryYear";
static const char kCardCardSecurityCode[] = "cardSecurityCode";
static const char kCardBillingAddress[] = "billingAddress";

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

PaymentCurrencyAmount::PaymentCurrencyAmount() {}
PaymentCurrencyAmount::~PaymentCurrencyAmount() = default;

bool PaymentCurrencyAmount::operator==(
    const PaymentCurrencyAmount& other) const {
  return this->currency == other.currency && this->value == other.value;
}

bool PaymentCurrencyAmount::operator!=(
    const PaymentCurrencyAmount& other) const {
  return !(*this == other);
}

PaymentItem::PaymentItem() {}
PaymentItem::~PaymentItem() = default;

bool PaymentItem::operator==(const PaymentItem& other) const {
  return this->label == other.label && this->amount == other.amount;
}

bool PaymentItem::operator!=(const PaymentItem& other) const {
  return !(*this == other);
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
         this->modifiers == other.modifiers;
}

bool PaymentDetails::operator!=(const PaymentDetails& other) const {
  return !(*this == other);
}

PaymentOptions::PaymentOptions()
    : request_payer_email(false),
      request_payer_phone(false),
      request_shipping(false) {}
PaymentOptions::~PaymentOptions() = default;

bool PaymentOptions::operator==(const PaymentOptions& other) const {
  return this->request_payer_email == other.request_payer_email &&
         this->request_payer_phone == other.request_payer_phone &&
         this->request_shipping == other.request_shipping;
}

bool PaymentOptions::operator!=(const PaymentOptions& other) const {
  return !(*this == other);
}

PaymentRequest::PaymentRequest() {}
PaymentRequest::PaymentRequest(const PaymentRequest& other) = default;
PaymentRequest::~PaymentRequest() = default;

bool PaymentRequest::operator==(const PaymentRequest& other) const {
  return this->payment_address == other.payment_address &&
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
  if (!value.GetList(kMethodData, &method_data_list) ||
      method_data_list->GetSize() == 0) {
    return false;
  }
  for (size_t i = 0; i < method_data_list->GetSize(); ++i) {
    const base::DictionaryValue* method_data_dict;
    // Method data is required.
    if (!method_data_list->GetDictionary(i, &method_data_dict))
      return false;

    PaymentMethodData method_data;
    const base::ListValue* supported_methods_list = nullptr;
    // At least one supported method is required.
    if (!method_data_dict->GetList(kSupportedMethods,
                                   &supported_methods_list) ||
        supported_methods_list->GetSize() == 0) {
      return false;
    }
    for (size_t i = 0; i < supported_methods_list->GetSize(); ++i) {
      base::string16 supported_method;
      supported_methods_list->GetString(i, &supported_method);
      method_data.supported_methods.push_back(supported_method);
    }
    method_data_dict->GetString(kData, &method_data.data);

    this->method_data.push_back(method_data);
  }

  // Parse the payment details.
  const base::DictionaryValue* payment_details_dict = nullptr;
  if (value.GetDictionary(kPaymentDetails, &payment_details_dict)) {
    const base::DictionaryValue* total_dict = nullptr;
    if (payment_details_dict->GetDictionary(kPaymentDetailsTotal,
                                            &total_dict)) {
      const base::DictionaryValue* amount_dict = nullptr;
      if (total_dict->GetDictionary(kPaymentDetailsTotalAmount, &amount_dict)) {
        amount_dict->GetString(kPaymentDetailsTotalAmountCurrency,
                               &this->details.total.amount.currency);
        amount_dict->GetString(kPaymentDetailsTotalAmountValue,
                               &this->details.total.amount.value);
      }
    }
  }

  // TODO(crbug.com/602666): Parse the remaining elements.

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
  if (!this->card_number.empty())
    result->SetString(kCardCardNumber, this->card_number);
  if (!this->expiry_month.empty())
    result->SetString(kCardExpiryMonth, this->expiry_month);
  if (!this->expiry_year.empty())
    result->SetString(kCardExpiryYear, this->expiry_year);
  if (!this->card_security_code.empty())
    result->SetString(kCardCardSecurityCode, this->card_security_code);
  result->Set(kCardBillingAddress, this->billing_address.ToDictionaryValue());

  return result;
}

PaymentResponse::PaymentResponse() {}
PaymentResponse::PaymentResponse(const PaymentResponse& other) = default;
PaymentResponse::~PaymentResponse() = default;

bool PaymentResponse::operator==(const PaymentResponse& other) const {
  return this->method_name == other.method_name &&
         this->details == other.details;
}

bool PaymentResponse::operator!=(const PaymentResponse& other) const {
  return !(*this == other);
}

std::unique_ptr<base::DictionaryValue> PaymentResponse::ToDictionaryValue()
    const {
  std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue());

  if (!this->method_name.empty())
    result->SetString(kMethodName, this->method_name);
  result->Set(kPaymentDetails, this->details.ToDictionaryValue());

  return result;
}

}  // namespace web
