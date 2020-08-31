// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_AUTOFILL_PAYMENT_APP_H_
#define COMPONENTS_PAYMENTS_CORE_AUTOFILL_PAYMENT_APP_H_

#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "build/build_config.h"
#include "components/autofill/core/browser/address_normalizer.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/full_card_request.h"
#include "components/payments/core/payment_app.h"

namespace payments {

class PaymentRequestBaseDelegate;

// Represents an autofill credit card in Payment Request.
class AutofillPaymentApp
    : public PaymentApp,
      public autofill::payments::FullCardRequest::ResultDelegate {
 public:
  // |billing_profiles| is owned by the caller and should outlive this object.
  // |payment_request_delegate| must outlive this object.
  AutofillPaymentApp(
      const std::string& method_name,
      const autofill::CreditCard& card,
      const std::vector<autofill::AutofillProfile*>& billing_profiles,
      const std::string& app_locale,
      PaymentRequestBaseDelegate* payment_request_delegate);
  ~AutofillPaymentApp() override;

  // PaymentApp:
  void InvokePaymentApp(PaymentApp::Delegate* delegate) override;
  bool IsCompleteForPayment() const override;
  uint32_t GetCompletenessScore() const override;
  bool CanPreselect() const override;
  base::string16 GetMissingInfoLabel() const override;
  bool HasEnrolledInstrument() const override;
  void RecordUse() override;
  bool NeedsInstallation() const override;
  base::string16 GetLabel() const override;
  base::string16 GetSublabel() const override;
  bool IsValidForModifier(
      const std::string& method,
      bool supported_networks_specified,
      const std::set<std::string>& supported_networks) const override;
  base::WeakPtr<PaymentApp> AsWeakPtr() override;
  bool HandlesShippingAddress() const override;
  bool HandlesPayerName() const override;
  bool HandlesPayerEmail() const override;
  bool HandlesPayerPhone() const override;

  // autofill::payments::FullCardRequest::ResultDelegate:
  void OnFullCardRequestSucceeded(
      const autofill::payments::FullCardRequest& full_card_request,
      const autofill::CreditCard& card,
      const base::string16& cvc) override;
  void OnFullCardRequestFailed() override;

  void RecordMissingFieldsForApp() const;

  // Sets whether the complete and valid autofill data for merchant's request is
  // available.
  void set_is_requested_autofill_data_available(bool available) {
    is_requested_autofill_data_available_ = available;
  }
  autofill::CreditCard* credit_card() { return &credit_card_; }
  const autofill::CreditCard* credit_card() const { return &credit_card_; }

  const std::string& method_name() const { return method_name_; }

 private:
  // Generates the basic card response and sends it to the delegate.
  void GenerateBasicCardResponse();

  // To be used as AddressNormalizer::NormalizationCallback.
  void OnAddressNormalized(bool success,
                           const autofill::AutofillProfile& normalized_profile);

  const std::string method_name_;

  // A copy of the card is owned by this object.
  autofill::CreditCard credit_card_;

  // Not owned by this object, should outlive this.
  const std::vector<autofill::AutofillProfile*>& billing_profiles_;

  const std::string app_locale_;

  PaymentApp::Delegate* delegate_;
  PaymentRequestBaseDelegate* payment_request_delegate_;
  autofill::AutofillProfile billing_address_;

  base::string16 cvc_;

  bool is_waiting_for_card_unmask_;
  bool is_waiting_for_billing_address_normalization_;

  // True when complete and valid autofill data for merchant's request is
  // available, e.g., if merchant specifies `requestPayerEmail: true`, then this
  // variable is true only if the autofill data contains a valid email address.
  bool is_requested_autofill_data_available_ = false;

  base::WeakPtrFactory<AutofillPaymentApp> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(AutofillPaymentApp);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_AUTOFILL_PAYMENT_APP_H_
