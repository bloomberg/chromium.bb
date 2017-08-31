// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_PAYMENT_DETAILS_MODIFIER_H_
#define COMPONENTS_PAYMENTS_CORE_PAYMENT_DETAILS_MODIFIER_H_

#include <memory>
#include <string>
#include <vector>

#include "components/payments/core/payment_item.h"
#include "components/payments/core/payment_method_data.h"

// C++ bindings for the PaymentRequest API PaymentDetailsModifier. Conforms to
// the following spec:
// https://w3c.github.io/payment-request/#dom-paymentdetailsmodifier

namespace base {
class DictionaryValue;
}

namespace payments {

// Details that modify the PaymentDetails based on the payment method
// identifier.
class PaymentDetailsModifier {
 public:
  PaymentDetailsModifier();
  PaymentDetailsModifier(const PaymentDetailsModifier& other);
  ~PaymentDetailsModifier();

  PaymentDetailsModifier& operator=(const PaymentDetailsModifier& other);
  bool operator==(const PaymentDetailsModifier& other) const;
  bool operator!=(const PaymentDetailsModifier& other) const;

  // Creates a base::DictionaryValue with the properties of this
  // PaymentDetailsModifier.
  std::unique_ptr<base::DictionaryValue> ToDictionaryValue() const;

  // A payment method identifier and any associated payment method specific
  // data. The remaining fields in the PaymentDetailsModifier apply only if the
  // user selects this payment method.
  PaymentMethodData method_data;

  // This value overrides the total field in the PaymentDetails dictionary for
  // the payment method identifiers in the supportedMethods field.
  std::unique_ptr<PaymentItem> total;

  // Provides additional display items that are appended to the displayItems
  // field in the PaymentDetails dictionary for the payment method identifiers
  // in the supportedMethods field. This field is commonly used to add a
  // discount or surcharge line item indicating the reason for the different
  // total amount for the selected payment method that the user agent may
  // display.
  std::vector<PaymentItem> additional_display_items;
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_PAYMENT_DETAILS_MODIFIER_H_
