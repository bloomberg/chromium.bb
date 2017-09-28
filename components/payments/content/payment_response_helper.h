// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CONTENT_PAYMENT_RESPONSE_HELPER_H_
#define COMPONENTS_PAYMENTS_CONTENT_PAYMENT_RESPONSE_HELPER_H_

#include "base/macros.h"
#include "components/autofill/core/browser/address_normalizer.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/payments/core/payment_instrument.h"
#include "third_party/WebKit/public/platform/modules/payments/payment_request.mojom.h"

namespace payments {

class PaymentRequestDelegate;
class PaymentRequestSpec;

// A helper class to facilitate the creation of the PaymentResponse.
class PaymentResponseHelper : public PaymentInstrument::Delegate,
                              public autofill::AddressNormalizer::Delegate {
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
                        PaymentRequestDelegate* payment_request_delegate,
                        autofill::AutofillProfile* selected_shipping_profile,
                        autofill::AutofillProfile* selected_contact_profile,
                        Delegate* delegate);
  ~PaymentResponseHelper() override;

  // Returns a new mojo PaymentAddress based on the specified
  // |profile| and |app_locale|.
  static mojom::PaymentAddressPtr GetMojomPaymentAddressFromAutofillProfile(
      const autofill::AutofillProfile& profile,
      const std::string& app_locale);

  // PaymentInstrument::Delegate
  void OnInstrumentDetailsReady(
      const std::string& method_name,
      const std::string& stringified_details) override;
  void OnInstrumentDetailsError() override {}

  // AddressNormalizer::Delegate
  void OnAddressNormalized(
      const autofill::AutofillProfile& normalized_profile) override;
  void OnCouldNotNormalize(const autofill::AutofillProfile& profile) override;

 private:
  // Generates the Payment Response and sends it to the delegate.
  void GeneratePaymentResponse();

  const std::string& app_locale_;
  bool is_waiting_for_shipping_address_normalization_;
  bool is_waiting_for_instrument_details_;

  // Not owned, cannot be null.
  PaymentRequestSpec* spec_;
  Delegate* delegate_;
  PaymentInstrument* selected_instrument_;
  PaymentRequestDelegate* payment_request_delegate_;

  // Not owned, can be null (dependent on the spec).
  autofill::AutofillProfile* selected_contact_profile_;

  // A normalized copy of the shipping address, which will be included in the
  // PaymentResponse.
  autofill::AutofillProfile shipping_address_;

  // Instrument Details.
  std::string method_name_;
  std::string stringified_details_;

  DISALLOW_COPY_AND_ASSIGN(PaymentResponseHelper);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CONTENT_PAYMENT_RESPONSE_HELPER_H_
