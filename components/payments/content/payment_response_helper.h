// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_PAYMENT_RESPONSE_HELPER_H_
#define COMPONENTS_PAYMENTS_CONTENT_PAYMENT_RESPONSE_HELPER_H_

#include "base/macros.h"
#include "components/payments/core/payment_instrument.h"
#include "components/payments/mojom/payment_request.mojom.h"

namespace autofill {
class AutofillProfile;
}  // namespace autofill

namespace payments {

class PaymentRequestSpec;

// TODO(sebsg): Asynchronously normalize the billing and shipping addresses
// before adding them to the PaymentResponse.
// A helper class to facilitate the creation of the PaymentResponse.
class PaymentResponseHelper : public PaymentInstrument::Delegate {
 public:
  class Delegate {
   public:
    virtual ~Delegate() {}

    virtual void OnPaymentResponseReady(
        mojom::PaymentResponsePtr payment_response) = 0;
  };

  // The spec, selected_instrument and delegate cannot be null.
  PaymentResponseHelper(const std::string& app_locale,
                        PaymentRequestSpec* spec,
                        PaymentInstrument* selected_instrument,
                        autofill::AutofillProfile* selected_shipping_profile,
                        autofill::AutofillProfile* selected_contact_profile,
                        Delegate* delegate);
  ~PaymentResponseHelper() override;

  // Returns a new mojo PaymentAddress based on the specified
  // |profile| and |app_locale|.
  static mojom::PaymentAddressPtr GetMojomPaymentAddressFromAutofillProfile(
      const autofill::AutofillProfile* const profile,
      const std::string& app_locale);

  // PaymentInstrument::Delegate
  void OnInstrumentDetailsReady(
      const std::string& method_name,
      const std::string& stringified_details) override;
  void OnInstrumentDetailsError() override {}

 private:
  const std::string& app_locale_;

  // Not owned, cannot be null.
  PaymentRequestSpec* spec_;
  Delegate* delegate_;
  PaymentInstrument* selected_instrument_;

  // Not owned, can be null (dependent on the spec).
  autofill::AutofillProfile* selected_shipping_profile_;
  autofill::AutofillProfile* selected_contact_profile_;

  DISALLOW_COPY_AND_ASSIGN(PaymentResponseHelper);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PAYMENT_RESPONSE_HELPER_H_
