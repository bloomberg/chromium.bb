// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PAYMENTS_CORE_AUTOFILL_PAYMENT_INSTRUMENT_H_
#define COMPONENTS_PAYMENTS_CORE_AUTOFILL_PAYMENT_INSTRUMENT_H_

#include <set>
#include <string>
#include <vector>

#include "base/macros.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/payments/core/payment_instrument.h"

namespace autofill {
class AutofillProfile;
}

namespace payments {

// Represents an Autofill/Payments credit card form of payment in Payment
// Request.
class AutofillPaymentInstrument : public PaymentInstrument {
 public:
  // |billing_profiles| is owned by the caller and should outlive this object.
  AutofillPaymentInstrument(
      const std::string& method_name,
      const autofill::CreditCard& card,
      const std::vector<autofill::AutofillProfile*>& billing_profiles,
      const std::string& app_locale);
  ~AutofillPaymentInstrument() override;

  // PaymentInstrument:
  void InvokePaymentApp(PaymentInstrument::Delegate* delegate) override;
  bool IsValid() override;

 private:
  // A copy of the card is owned by this object.
  const autofill::CreditCard credit_card_;
  // Not owned by this object, should outlive this.
  const std::vector<autofill::AutofillProfile*>& billing_profiles_;

  const std::string app_locale_;

  DISALLOW_COPY_AND_ASSIGN(AutofillPaymentInstrument);
};

}  // namespace payments

#endif  // COMPONENTS_PAYMENTS_CORE_AUTOFILL_PAYMENT_INSTRUMENT_H_
