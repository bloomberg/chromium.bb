// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/autofill_payment_instrument.h"

#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/payments/core/basic_card_response.h"
#include "components/payments/core/payment_request_data_util.h"

namespace payments {

AutofillPaymentInstrument::AutofillPaymentInstrument(
    const std::string& method_name,
    const autofill::CreditCard& card,
    const std::vector<autofill::AutofillProfile*>& billing_profiles,
    const std::string& app_locale)
    : PaymentInstrument(
          method_name,
          /* label= */ card.TypeAndLastFourDigits(),
          /* sublabel= */
          card.GetInfo(autofill::AutofillType(autofill::CREDIT_CARD_NAME_FULL),
                       app_locale),
          autofill::data_util::GetPaymentRequestData(card.type())
              .icon_resource_id),
      credit_card_(card),
      billing_profiles_(billing_profiles),
      app_locale_(app_locale) {}
AutofillPaymentInstrument::~AutofillPaymentInstrument() {}

void AutofillPaymentInstrument::InvokePaymentApp(
    PaymentInstrument::Delegate* delegate) {
  DCHECK(delegate);
  std::string stringified_details;
  // TODO(mathp): Show the CVC dialog and use the data instead of the
  // placeholder string.
  std::unique_ptr<base::DictionaryValue> response_value =
      payments::data_util::GetBasicCardResponseFromAutofillCreditCard(
          credit_card_, base::ASCIIToUTF16("123"), billing_profiles_,
          app_locale_)
          .ToDictionaryValue();
  base::JSONWriter::Write(*response_value, &stringified_details);
  delegate->OnInstrumentDetailsReady(method_name(), stringified_details);
}

bool AutofillPaymentInstrument::IsValid() {
  return credit_card_.IsValid();
}

}  // namespace payments
