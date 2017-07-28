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
#include "components/payments/core/payment_method_data.h"
#include "components/payments/core/payment_options_provider.h"

// C++ bindings for the PaymentRequest API. Conforms to the following specs:
// https://w3c.github.io/browser-payment-api/ (18 July 2016 editor's draft)
// https://w3c.github.io/webpayments-methods-card/ (31 May 2016 editor's draft)

namespace base {
class DictionaryValue;
}

namespace web {

// Supplies monetary amounts.
class PaymentCurrencyAmount {
 public:
  PaymentCurrencyAmount();
  ~PaymentCurrencyAmount();

  bool operator==(const PaymentCurrencyAmount& other) const;
  bool operator!=(const PaymentCurrencyAmount& other) const;

  // Populates the properties of this PaymentCurrencyAmount from |value|.
  // Returns true if the required values are present.
  bool FromDictionaryValue(const base::DictionaryValue& value);

  // Creates a base::DictionaryValue with the properties of this
  // PaymentCurrencyAmount.
  std::unique_ptr<base::DictionaryValue> ToDictionaryValue() const;

  // A currency identifier. The most common identifiers are three-letter
  // alphabetic codes as defined by ISO 4217 (for example, "USD" for US Dollars)
  // however any string is considered valid.
  base::string16 currency;

  // A string containing the decimal monetary value.
  base::string16 value;

  // A URL that indicates the currency system that the currency identifier
  // belongs to.
  base::string16 currency_system;
};

// Information indicating what the payment request is for and the value asked
// for.
class PaymentItem {
 public:
  PaymentItem();
  ~PaymentItem();

  bool operator==(const PaymentItem& other) const;
  bool operator!=(const PaymentItem& other) const;

  // Populates the properties of this PaymentItem from |value|. Returns true if
  // the required values are present.
  bool FromDictionaryValue(const base::DictionaryValue& value);

  // Creates a base::DictionaryValue with the properties of this
  // PaymentItem.
  std::unique_ptr<base::DictionaryValue> ToDictionaryValue() const;

  // A human-readable description of the item.
  base::string16 label;

  // The monetary amount for the item.
  PaymentCurrencyAmount amount;

  // When set to true this flag means that the amount field is not final. This
  // is commonly used to show items such as shipping or tax amounts that depend
  // upon selection of shipping address or shipping option.
  bool pending;
};

// Information describing a shipping option.
class PaymentShippingOption {
 public:
  PaymentShippingOption();
  PaymentShippingOption(const PaymentShippingOption& other);
  ~PaymentShippingOption();

  bool operator==(const PaymentShippingOption& other) const;
  bool operator!=(const PaymentShippingOption& other) const;

  // Populates the properties of this PaymentShippingOption from |value|.
  // Returns true if the required values are present.
  bool FromDictionaryValue(const base::DictionaryValue& value);

  // An identifier used to reference this PaymentShippingOption. It is unique
  // for a given PaymentRequest.
  base::string16 id;

  // A human-readable description of the item. The user agent should use this
  // string to display the shipping option to the user.
  base::string16 label;

  // A PaymentCurrencyAmount containing the monetary amount for the option.
  PaymentCurrencyAmount amount;

  // This is set to true to indicate that this is the default selected
  // PaymentShippingOption in a sequence. User agents should display this option
  // by default in the user interface.
  bool selected;
};

// Details that modify the PaymentDetails based on the payment method
// identifier.
class PaymentDetailsModifier {
 public:
  PaymentDetailsModifier();
  PaymentDetailsModifier(const PaymentDetailsModifier& other);
  ~PaymentDetailsModifier();

  bool operator==(const PaymentDetailsModifier& other) const;
  bool operator!=(const PaymentDetailsModifier& other) const;

  // Creates a base::DictionaryValue with the properties of this
  // PaymentDetailsModifier.
  std::unique_ptr<base::DictionaryValue> ToDictionaryValue() const;

  // A sequence of payment method identifiers. The remaining fields in the
  // PaymentDetailsModifier apply only if the user selects a payment method
  // included in this sequence.
  std::vector<base::string16> supported_methods;

  // This value overrides the total field in the PaymentDetails dictionary for
  // the payment method identifiers in the supportedMethods field.
  PaymentItem total;

  // Provides additional display items that are appended to the displayItems
  // field in the PaymentDetails dictionary for the payment method identifiers
  // in the supportedMethods field. This field is commonly used to add a
  // discount or surcharge line item indicating the reason for the different
  // total amount for the selected payment method that the user agent may
  // display.
  std::vector<PaymentItem> additional_display_items;
};

// Details about the requested transaction.
class PaymentDetails {
 public:
  PaymentDetails();
  PaymentDetails(const PaymentDetails& other);
  ~PaymentDetails();

  bool operator==(const PaymentDetails& other) const;
  bool operator!=(const PaymentDetails& other) const;

  // Populates the properties of this PaymentDetails from |value|. Returns true
  // if the required values are present. If |requires_total| is true, the total
  // property has to be present.
  bool FromDictionaryValue(const base::DictionaryValue& value,
                           bool requires_total);

  // The unique free-form identifier for this payment request.
  std::string id;

  // The total amount of the payment request.
  PaymentItem total;

  // Line items for the payment request that the user agent may display. For
  // example, it might include details of products or breakdown of tax and
  // shipping.
  std::vector<PaymentItem> display_items;

  // The different shipping options for the user to choose from. If empty, this
  // indicates that the merchant cannot ship to the current shipping address.
  std::vector<PaymentShippingOption> shipping_options;

  // Modifiers for particular payment method identifiers. For example, it allows
  // adjustment to the total amount based on payment method.
  std::vector<PaymentDetailsModifier> modifiers;

  // If non-empty, this is the error message the user agent should display to
  // the user when the payment request is updated using updateWith.
  base::string16 error;
};

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
  base::string16 shipping_option;

  // Properties set via the constructor for communicating from the page to the
  // browser UI.
  std::vector<payments::PaymentMethodData> method_data;
  PaymentDetails details;
  PaymentOptions options;
};

// Information provided in the Promise returned by a call to
// PaymentRequest.show().
class PaymentResponse {
 public:
  PaymentResponse();
  PaymentResponse(const PaymentResponse& other);
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
  payments::PaymentAddress shipping_address;

  // If the request_shipping flag was set to true in the PaymentOptions passed
  // to the PaymentRequest constructor, this will be the id attribute of the
  // selected shipping option.
  base::string16 shipping_option;

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
