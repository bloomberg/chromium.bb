// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_WEB_PUBLIC_PAYMENTS_PAYMENT_REQUEST_H_
#define IOS_WEB_PUBLIC_PAYMENTS_PAYMENT_REQUEST_H_

#include <memory>
#include <vector>

#include "base/strings/string16.h"

// C++ bindings for the PaymentRequest API. Conforms to the following specs:
// https://w3c.github.io/browser-payment-api/ (18 July 2016 editor's draft)
// https://w3c.github.io/webpayments-methods-card/ (31 May 2016 editor's draft)

namespace base {
class DictionaryValue;
}

namespace web {

// A shipping or billing address.
class PaymentAddress {
 public:
  PaymentAddress();
  PaymentAddress(const PaymentAddress& other);
  ~PaymentAddress();

  bool operator==(const PaymentAddress& other) const;
  bool operator!=(const PaymentAddress& other) const;

  // Populates |value| with the properties of this PaymentAddress.
  std::unique_ptr<base::DictionaryValue> ToDictionaryValue() const;

  // The CLDR (Common Locale Data Repository) region code. For example, US, GB,
  // CN, or JP.
  base::string16 country;

  // The most specific part of the address. It can include, for example, a
  // street name, a house number, apartment number, a rural delivery route,
  // descriptive instructions, or a post office box number.
  std::vector<base::string16> address_line;

  // The top level administrative subdivision of the country. For example, this
  // can be a state, a province, an oblast, or a prefecture.
  base::string16 region;

  // The city/town portion of the address.
  base::string16 city;

  // The dependent locality or sublocality within a city. For example, used for
  // neighborhoods, boroughs, districts, or UK dependent localities.
  base::string16 dependent_locality;

  // The postal code or ZIP code, also known as PIN code in India.
  base::string16 postal_code;

  // The sorting code as used in, for example, France.
  base::string16 sorting_code;

  // The BCP-47 language code for the address. It's used to determine the field
  // separators and the order of fields when formatting the address for display.
  base::string16 language_code;

  // The organization, firm, company, or institution at this address.
  base::string16 organization;

  // The name of the recipient or contact person.
  base::string16 recipient;

  // The name of an intermediary party or entity responsible for transferring
  // packages between the postal service and the recipient.
  base::string16 care_of;

  // The phone number of the recipient or contact person.
  base::string16 phone;
};

// A set of supported payment methods and any associated payment method specific
// data for those methods.
class PaymentMethodData {
 public:
  PaymentMethodData();
  PaymentMethodData(const PaymentMethodData& other);
  ~PaymentMethodData();

  bool operator==(const PaymentMethodData& other) const;
  bool operator!=(const PaymentMethodData& other) const;

  // Populates the properties of this PaymentMethodData from |value|. Returns
  // true if the required values are present.
  bool FromDictionaryValue(const base::DictionaryValue& value);

  // Payment method identifiers for payment methods that the merchant web site
  // accepts.
  std::vector<base::string16> supported_methods;

  // A JSON-serialized object that provides optional information that might be
  // needed by the supported payment methods.
  base::string16 data;
};

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

  // A currency identifier. The most common identifiers are three-letter
  // alphabetic codes as defined by ISO 4217 (for example, "USD" for US Dollars)
  // however any string is considered valid.
  base::string16 currency;

  // A string containing the decimal monetary value.
  base::string16 value;
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
  // if the required values are present.
  bool FromDictionaryValue(const base::DictionaryValue& value);

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
};

// Information describing a shipping option.
class PaymentOptions {
 public:
  PaymentOptions();
  ~PaymentOptions();

  bool operator==(const PaymentOptions& other) const;
  bool operator!=(const PaymentOptions& other) const;

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

  // Properties set in order to communicate user choices back to the page.
  PaymentAddress payment_address;
  base::string16 shipping_option;

  // Properties set via the constructor for communicating from the page to the
  // browser UI.
  std::vector<PaymentMethodData> method_data;
  PaymentDetails details;
  PaymentOptions options;
};

// Contains the response from the PaymentRequest API when a user accepts
// payment with a Basic Payment Card payment method (which is currently the only
// method supported on iOS).
class BasicCardResponse {
 public:
  BasicCardResponse();
  BasicCardResponse(const BasicCardResponse& other);
  ~BasicCardResponse();

  bool operator==(const BasicCardResponse& other) const;
  bool operator!=(const BasicCardResponse& other) const;

  // Populates |value| with the properties of this BasicCardResponse.
  std::unique_ptr<base::DictionaryValue> ToDictionaryValue() const;

  // The cardholder's name as it appears on the card.
  base::string16 cardholder_name;

  // The primary account number (PAN) for the payment card.
  base::string16 card_number;

  // A two-digit string for the expiry month of the card in the range 01 to 12.
  base::string16 expiry_month;

  // A two-digit string for the expiry year of the card in the range 00 to 99.
  base::string16 expiry_year;

  // A three or four digit string for the security code of the card (sometimes
  // known as the CVV, CVC, CVN, CVE or CID).
  base::string16 card_security_code;

  // The billing address information associated with the payment card.
  PaymentAddress billing_address;
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

  // The payment method identifier for the payment method that the user selected
  // to fulfil the transaction.
  base::string16 method_name;

  // A credit card response object used by the merchant to process the
  // transaction and determine successful fund transfer.
  BasicCardResponse details;
};

}  // namespace web

#endif  // IOS_WEB_PUBLIC_PAYMENTS_PAYMENT_REQUEST_H_
