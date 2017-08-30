// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_PAYMENTS_PAYMENT_REQUEST_H_
#define IOS_WEB_PUBLIC_PAYMENTS_PAYMENT_REQUEST_H_

#include <memory>
#include <string>
#include <vector>

#include "base/strings/string16.h"
#include "components/payments/core/basic_card_response.h"
#include "components/payments/core/payment_address.h"
#include "components/payments/core/payment_details.h"
#include "components/payments/core/payment_method_data.h"
#include "components/payments/core/payment_options_provider.h"

// TODO(crbug.com/759167): Web layer is not supposed to depend on components.
// Move definitions and implementations of PaymentOptions, PaymentRequest, and
// PaymentResponse to components/payments/core or ios/chrome/browser/payments.

// C++ bindings for the PaymentRequest API. Conforms to the following specs:
// https://w3c.github.io/browser-payment-api/ (18 July 2016 editor's draft)
// https://w3c.github.io/webpayments-methods-card/ (31 May 2016 editor's draft)

namespace base {
class DictionaryValue;
}

namespace web {

// These constants are for Univesral Link query parameters when receiving
// payment response data from an external application.
extern const char kPaymentRequestIDExternal[];
extern const char kPaymentRequestDataExternal[];

// Information describing a shipping option.
class PaymentOptions {
 public:
  PaymentOptions();
  ~PaymentOptions();

  bool operator==(const PaymentOptions& other) const;
  bool operator!=(const PaymentOptions& other) const;

  // Populates the properties of this PaymentOptions from |value|. Returns true
  // if the required values are present.
  bool FromDictionaryValue(const base::DictionaryValue& value);

  // Indicates whether the user agent should collect and return the payer's name
  // as part of the payment request. For example, this would be set to true to
  // allow a merchant to make a booking in the payer's name.
  bool request_payer_name;

  // Indicates whether the user agent should collect and return the payer's
  // email address as part of the payment request. For example, this would be
  // set to true to allow a merchant to email a receipt.
  bool request_payer_email;

  // Indicates whether the user agent should collect and return the payer's
  // phone number as part of the payment request. For example, this would be set
  // to true to allow a merchant to phone a customer with a billing enquiry.
  bool request_payer_phone;

  // Indicates whether the user agent should collect and return a shipping
  // address as part of the payment request. For example, this would be set to
  // true when physical goods need to be shipped by the merchant to the user.
  // This would be set to false for an online-only electronic purchase
  // transaction.
  bool request_shipping;

  // If request_shipping is set to true, then this field may only be used to
  // influence the way the user agent presents the user interface for gathering
  // the shipping address.
  payments::PaymentShippingType shipping_type;
};

// All of the information provided by a page making a request for payment.
class PaymentRequest {
 public:
  PaymentRequest();
  PaymentRequest(const PaymentRequest& other);
  ~PaymentRequest();

  bool operator==(const PaymentRequest& other) const;
  bool operator!=(const PaymentRequest& other) const;

  // Populates the properties of this PaymentRequest from |value|. Returns true
  // if the required values are present.
  bool FromDictionaryValue(const base::DictionaryValue& value);

  // The unique ID for this PaymentRequest. If it is not provided during
  // construction, one is generated.
  std::string payment_request_id;

  // Properties set in order to communicate user choices back to the page.
  payments::PaymentAddress shipping_address;
  std::string shipping_option;

  // Properties set via the constructor for communicating from the page to the
  // browser UI.
  std::vector<payments::PaymentMethodData> method_data;
  payments::PaymentDetails details;
  PaymentOptions options;
};

// Information provided in the Promise returned by a call to
// PaymentRequest.show().
class PaymentResponse {
 public:
  PaymentResponse();
  ~PaymentResponse();

  bool operator==(const PaymentResponse& other) const;
  bool operator!=(const PaymentResponse& other) const;

  // Populates |value| with the properties of this PaymentResponse.
  std::unique_ptr<base::DictionaryValue> ToDictionaryValue() const;

  // The same ID present in the original PaymentRequest.
  std::string payment_request_id;

  // The payment method identifier for the payment method that the user selected
  // to fulfil the transaction.
  std::string method_name;

  // The json-serialized stringified details of the payment method. Used by the
  // merchant to process the transaction and determine successful fund transfer.
  std::string details;

  // If request_shipping was set to true in the PaymentOptions passed to the
  // PaymentRequest constructor, this will be the full and final shipping
  // address chosen by the user.
  std::unique_ptr<payments::PaymentAddress> shipping_address;

  // If the request_shipping flag was set to true in the PaymentOptions passed
  // to the PaymentRequest constructor, this will be the id attribute of the
  // selected shipping option.
  std::string shipping_option;

  // If the request_payer_name flag was set to true in the PaymentOptions passed
  // to the PaymentRequest constructor, this will be the name provided by the
  // user.
  base::string16 payer_name;

  // If the request_payer_email flag was set to true in the PaymentOptions
  // passed to the PaymentRequest constructor, this will be the email address
  // chosen by the user.
  base::string16 payer_email;

  // If the request_payer_phone flag was set to true in the PaymentOptions
  // passed to the PaymentRequest constructor, this will be the phone number
  // chosen by the user.
  base::string16 payer_phone;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_PAYMENTS_PAYMENT_REQUEST_H_
