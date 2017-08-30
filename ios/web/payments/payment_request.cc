// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/web/public/payments/payment_request.h"

#include "base/json/json_reader.h"
#include "base/memory/ptr_util.h"
#include "base/values.h"

namespace {

// All of these are defined here (even though most are only used once each) so
// the format details are easy to locate and update or compare to the spec doc.
// (https://w3c.github.io/browser-payment-api/).
static const char kPaymentOptionsRequestPayerEmail[] = "requestPayerEmail";
static const char kPaymentOptionsRequestPayerName[] = "requestPayerName";
static const char kPaymentOptionsRequestPayerPhone[] = "requestPayerPhone";
static const char kPaymentOptionsRequestShipping[] = "requestShipping";
static const char kPaymentOptionsShippingTypeDelivery[] = "delivery";
static const char kPaymentOptionsShippingTypePickup[] = "pickup";
static const char kPaymentOptionsShippingType[] = "shippingType";
static const char kPaymentRequestDetails[] = "details";
static const char kPaymentRequestId[] = "id";
static const char kPaymentRequestMethodData[] = "methodData";
static const char kPaymentRequestOptions[] = "options";
static const char kPaymentResponseId[] = "requestId";
static const char kPaymentResponseDetails[] = "details";
static const char kPaymentResponseMethodName[] = "methodName";
static const char kPaymentResponsePayerEmail[] = "payerEmail";
static const char kPaymentResponsePayerName[] = "payerName";
static const char kPaymentResponsePayerPhone[] = "payerPhone";
static const char kPaymentResponseShippingAddress[] = "shippingAddress";
static const char kPaymentResponseShippingOption[] = "shippingOption";

}  // namespace

namespace web {

const char kPaymentRequestIDExternal[] = "payment-request-id";
const char kPaymentRequestDataExternal[] = "payment-request-data";

PaymentOptions::PaymentOptions()
    : request_payer_name(false),
      request_payer_email(false),
      request_payer_phone(false),
      request_shipping(false),
      shipping_type(payments::PaymentShippingType::SHIPPING) {}
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

  std::string shipping_type;
  value.GetString(kPaymentOptionsShippingType, &shipping_type);
  if (shipping_type == kPaymentOptionsShippingTypeDelivery) {
    this->shipping_type = payments::PaymentShippingType::DELIVERY;
  } else if (shipping_type == kPaymentOptionsShippingTypePickup) {
    this->shipping_type = payments::PaymentShippingType::PICKUP;
  } else {
    this->shipping_type = payments::PaymentShippingType::SHIPPING;
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

  if (!value.GetString(kPaymentRequestId, &this->payment_request_id)) {
    return false;
  }

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

    payments::PaymentMethodData method_data;
    if (!method_data.FromDictionaryValue(*method_data_dict))
      return false;
    this->method_data.push_back(method_data);
  }

  // Parse the payment details.
  const base::DictionaryValue* payment_details_dict = nullptr;
  if (!value.GetDictionary(kPaymentRequestDetails, &payment_details_dict) ||
      !this->details.FromDictionaryValue(*payment_details_dict,
                                         /*requires_total=*/true)) {
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

PaymentResponse::PaymentResponse() {}
PaymentResponse::~PaymentResponse() = default;

bool PaymentResponse::operator==(const PaymentResponse& other) const {
  return this->payment_request_id == other.payment_request_id &&
         this->method_name == other.method_name &&
         this->details == other.details &&
         ((!this->shipping_address && !other.shipping_address) ||
          (this->shipping_address && other.shipping_address &&
           *this->shipping_address == *other.shipping_address)) &&
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

  result->SetString(kPaymentResponseId, this->payment_request_id);
  result->SetString(kPaymentResponseMethodName, this->method_name);
  // |this.details| is a json-serialized string. Parse it to a base::Value so
  // that when |result| is converted to a JSON string, the "details" property
  // won't get json-escaped.
  std::unique_ptr<base::Value> details =
      base::JSONReader().ReadToValue(this->details);
  result->Set(kPaymentResponseDetails,
              details ? std::move(details) : base::MakeUnique<base::Value>());
  result->Set(kPaymentResponseShippingAddress,
              this->shipping_address
                  ? this->shipping_address->ToDictionaryValue()
                  : base::MakeUnique<base::Value>());
  result->SetString(kPaymentResponseShippingOption, this->shipping_option);
  result->SetString(kPaymentResponsePayerName, this->payer_name);
  result->SetString(kPaymentResponsePayerEmail, this->payer_email);
  result->SetString(kPaymentResponsePayerPhone, this->payer_phone);
  return result;
}

}  // namespace web
