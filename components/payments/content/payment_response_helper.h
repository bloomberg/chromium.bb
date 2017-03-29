// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_PAYMENT_RESPONSE_HELPER_H_
#define COMPONENTS_PAYMENTS_CONTENT_PAYMENT_RESPONSE_HELPER_H_

#include "base/macros.h"
#include "components/payments/content/payment_request.mojom.h"

namespace autofill {
class AutofillProfile;
}  // namespace autofill

namespace payments {

// TODO(sebsg): Accept PaymentInstrument and handle generating the payment
// aspect of the PaymentResponse in this class.
// TODO(sebsg): Asynchronously normalize the billing and shipping addresses
// before adding them to the PaymentResponse.
// A helper class to facilitate the creation of the PaymentResponse.
class PaymentResponseHelper {
 public:
  PaymentResponseHelper();
  ~PaymentResponseHelper();

  static mojom::PaymentAddressPtr GetMojomPaymentAddressFromAutofillProfile(
      const autofill::AutofillProfile* const profile,
      const std::string& app_locale);

 private:
  DISALLOW_COPY_AND_ASSIGN(PaymentResponseHelper);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PAYMENT_RESPONSE_HELPER_H_
