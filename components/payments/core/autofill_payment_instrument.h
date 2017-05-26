// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_AUTOFILL_PAYMENT_INSTRUMENT_H_
#define COMPONENTS_PAYMENTS_CORE_AUTOFILL_PAYMENT_INSTRUMENT_H_

#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/payments/full_card_request.h"
#include "components/payments/core/address_normalizer.h"
#include "components/payments/core/payment_instrument.h"

namespace payments {

class PaymentRequestDelegate;

// Represents an Autofill/Payments credit card form of payment in Payment
// Request.
class AutofillPaymentInstrument
    : public PaymentInstrument,
      public autofill::payments::FullCardRequest::ResultDelegate,
      public AddressNormalizer::Delegate {
 public:
  // |billing_profiles| is owned by the caller and should outlive this object.
  // |payment_request_delegate| must outlive this object.
  AutofillPaymentInstrument(
      const std::string& method_name,
      const autofill::CreditCard& card,
      const std::vector<autofill::AutofillProfile*>& billing_profiles,
      const std::string& app_locale,
      PaymentRequestDelegate* payment_request_delegate);
  ~AutofillPaymentInstrument() override;

  // PaymentInstrument:
  void InvokePaymentApp(PaymentInstrument::Delegate* delegate) override;
  bool IsCompleteForPayment() override;
  base::string16 GetMissingInfoLabel() override;
  bool IsValidForCanMakePayment() override;
  void RecordUse() override;
  base::string16 GetLabel() const override;
  base::string16 GetSublabel() const override;

  // autofill::payments::FullCardRequest::ResultDelegate:
  void OnFullCardRequestSucceeded(const autofill::CreditCard& card,
                                  const base::string16& cvc) override;
  void OnFullCardRequestFailed() override;

  // AddressNormalizer::Delegate:
  void OnAddressNormalized(
      const autofill::AutofillProfile& normalized_profile) override;
  void OnCouldNotNormalize(const autofill::AutofillProfile& profile) override;

  autofill::CreditCard* credit_card() { return &credit_card_; }

 private:
  // Generates the basic card response and sends it to the delegate.
  void GenerateBasicCardResponse();

  // A copy of the card is owned by this object.
  autofill::CreditCard credit_card_;
  // Not owned by this object, should outlive this.
  const std::vector<autofill::AutofillProfile*>& billing_profiles_;

  const std::string app_locale_;

  PaymentInstrument::Delegate* delegate_;
  PaymentRequestDelegate* payment_request_delegate_;
  autofill::AutofillProfile billing_address_;

  base::string16 cvc_;

  bool is_waiting_for_card_unmask_;
  bool is_waiting_for_billing_address_normalization_;

  base::WeakPtrFactory<AutofillPaymentInstrument> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AutofillPaymentInstrument);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_AUTOFILL_PAYMENT_INSTRUMENT_H_
