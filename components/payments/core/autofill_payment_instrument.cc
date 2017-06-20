// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/core/autofill_payment_instrument.h"

#include <memory>

#include "base/json/json_writer.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/autofill/core/browser/autofill_country.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/payments/core/basic_card_response.h"
#include "components/payments/core/payment_request_data_util.h"
#include "components/payments/core/payment_request_delegate.h"

namespace payments {

AutofillPaymentInstrument::AutofillPaymentInstrument(
    const std::string& method_name,
    const autofill::CreditCard& card,
    const std::vector<autofill::AutofillProfile*>& billing_profiles,
    const std::string& app_locale,
    PaymentRequestDelegate* payment_request_delegate)
    : PaymentInstrument(
          method_name,
          autofill::data_util::GetPaymentRequestData(card.network())
              .icon_resource_id,
          PaymentInstrument::Type::AUTOFILL),
      credit_card_(card),
      billing_profiles_(billing_profiles),
      app_locale_(app_locale),
      delegate_(nullptr),
      payment_request_delegate_(payment_request_delegate),
      weak_ptr_factory_(this) {}
AutofillPaymentInstrument::~AutofillPaymentInstrument() {}

void AutofillPaymentInstrument::InvokePaymentApp(
    PaymentInstrument::Delegate* delegate) {
  DCHECK(delegate);
  // There can be only one FullCardRequest going on at a time. If |delegate_| is
  // not null, there's already an active request, which shouldn't happen.
  // |delegate_| is reset to nullptr when the request succeeds or fails.
  DCHECK(!delegate_);
  delegate_ = delegate;

  // Get the billing address.
  if (!credit_card_.billing_address_id().empty()) {
    autofill::AutofillProfile* billing_address =
        autofill::PersonalDataManager::GetProfileFromProfilesByGUID(
            credit_card_.billing_address_id(), billing_profiles_);
    if (billing_address)
      billing_address_ = *billing_address;
  }

  is_waiting_for_billing_address_normalization_ = true;
  is_waiting_for_card_unmask_ = true;

  // Start the normalization of the billing address.
  // Use the country code from the profile if it is set, otherwise infer it
  // from the |app_locale_|.
  std::string country_code = base::UTF16ToUTF8(
      billing_address_.GetRawInfo(autofill::ADDRESS_HOME_COUNTRY));
  if (!autofill::data_util::IsValidCountryCode(country_code)) {
    country_code = autofill::AutofillCountry::CountryCodeForLocale(app_locale_);
  }
  payment_request_delegate_->GetAddressNormalizer()->StartAddressNormalization(
      billing_address_, country_code, /*timeout_seconds=*/5, this);

  payment_request_delegate_->DoFullCardRequest(credit_card_,
                                               weak_ptr_factory_.GetWeakPtr());
}

bool AutofillPaymentInstrument::IsCompleteForPayment() {
  // COMPLETE or EXPIRED cards are considered valid for payment. The user will
  // be prompted to enter the new expiration at the CVC step.
  return autofill::GetCompletionStatusForCard(credit_card_, app_locale_,
                                              billing_profiles_) <=
         autofill::CREDIT_CARD_EXPIRED;
}

base::string16 AutofillPaymentInstrument::GetMissingInfoLabel() {
  return autofill::GetCompletionMessageForCard(
      autofill::GetCompletionStatusForCard(credit_card_, app_locale_,
                                           billing_profiles_));
}

bool AutofillPaymentInstrument::IsValidForCanMakePayment() {
  autofill::CreditCardCompletionStatus status =
      autofill::GetCompletionStatusForCard(credit_card_, app_locale_,
                                           billing_profiles_);
  // Card has to have a cardholder name and number for the purposes of
  // CanMakePayment. An expired card is still valid at this stage.
  return !(status & autofill::CREDIT_CARD_NO_CARDHOLDER ||
           status & autofill::CREDIT_CARD_NO_NUMBER);
}

void AutofillPaymentInstrument::RecordUse() {
  // Record the use of the credit card.
  payment_request_delegate_->GetPersonalDataManager()->RecordUseOf(
      credit_card_);
}

base::string16 AutofillPaymentInstrument::GetLabel() const {
  return credit_card_.NetworkAndLastFourDigits();
}

base::string16 AutofillPaymentInstrument::GetSublabel() const {
  return credit_card_.GetInfo(
      autofill::AutofillType(autofill::CREDIT_CARD_NAME_FULL), app_locale_);
}

void AutofillPaymentInstrument::OnFullCardRequestSucceeded(
    const autofill::CreditCard& card,
    const base::string16& cvc) {
  DCHECK(delegate_);
  credit_card_ = card;
  cvc_ = cvc;
  is_waiting_for_card_unmask_ = false;

  if (!is_waiting_for_billing_address_normalization_)
    GenerateBasicCardResponse();
}

void AutofillPaymentInstrument::OnFullCardRequestFailed() {
  // The user may have cancelled the unmask or something has gone wrong (e.g.,
  // the network request failed). In all cases, reset the |delegate_| so another
  // request can start.
  delegate_ = nullptr;
}

void AutofillPaymentInstrument::OnAddressNormalized(
    const autofill::AutofillProfile& normalized_profile) {
  DCHECK(is_waiting_for_billing_address_normalization_);

  billing_address_ = normalized_profile;
  is_waiting_for_billing_address_normalization_ = false;

  if (!is_waiting_for_card_unmask_)
    GenerateBasicCardResponse();
}

void AutofillPaymentInstrument::OnCouldNotNormalize(
    const autofill::AutofillProfile& profile) {
  // Since the phone number is formatted in either case, this profile should be
  // used.
  OnAddressNormalized(profile);
}

void AutofillPaymentInstrument::GenerateBasicCardResponse() {
  DCHECK(!is_waiting_for_billing_address_normalization_);
  DCHECK(!is_waiting_for_card_unmask_);
  DCHECK(delegate_);

  std::unique_ptr<base::DictionaryValue> response_value =
      payments::data_util::GetBasicCardResponseFromAutofillCreditCard(
          credit_card_, cvc_, billing_address_, app_locale_)
          .ToDictionaryValue();
  std::string stringified_details;
  base::JSONWriter::Write(*response_value, &stringified_details);
  delegate_->OnInstrumentDetailsReady(method_name(), stringified_details);

  delegate_ = nullptr;
  cvc_ = base::UTF8ToUTF16("");
}

}  // namespace payments
