// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_response_helper.h"

#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/payments/content/payment_request_spec.h"
#include "third_party/libphonenumber/phonenumber_api.h"

namespace payments {

namespace {

using ::i18n::phonenumbers::PhoneNumberUtil;

}  // namespace

PaymentResponseHelper::PaymentResponseHelper(
    const std::string& app_locale,
    PaymentRequestSpec* spec,
    PaymentInstrument* selected_instrument,
    autofill::AutofillProfile* selected_shipping_profile,
    autofill::AutofillProfile* selected_contact_profile,
    Delegate* delegate)
    : app_locale_(app_locale),
      spec_(spec),
      delegate_(delegate),
      selected_instrument_(selected_instrument),
      selected_shipping_profile_(selected_shipping_profile),
      selected_contact_profile_(selected_contact_profile) {
  DCHECK(spec_);
  DCHECK(selected_instrument_);
  DCHECK(delegate_);

  // Start to get the instrument details. Will call back into
  // OnInstrumentDetailsReady.
  selected_instrument_->InvokePaymentApp(this);
};

PaymentResponseHelper::~PaymentResponseHelper(){};

// static
mojom::PaymentAddressPtr
PaymentResponseHelper::GetMojomPaymentAddressFromAutofillProfile(
    const autofill::AutofillProfile* const profile,
    const std::string& app_locale) {
  mojom::PaymentAddressPtr payment_address = mojom::PaymentAddress::New();

  payment_address->country =
      base::UTF16ToUTF8(profile->GetRawInfo(autofill::ADDRESS_HOME_COUNTRY));
  payment_address->address_line = base::SplitString(
      base::UTF16ToUTF8(profile->GetInfo(
          autofill::AutofillType(autofill::ADDRESS_HOME_STREET_ADDRESS),
          app_locale)),
      "\n", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  payment_address->region =
      base::UTF16ToUTF8(profile->GetRawInfo(autofill::ADDRESS_HOME_STATE));
  payment_address->city =
      base::UTF16ToUTF8(profile->GetRawInfo(autofill::ADDRESS_HOME_CITY));
  payment_address->dependent_locality = base::UTF16ToUTF8(
      profile->GetRawInfo(autofill::ADDRESS_HOME_DEPENDENT_LOCALITY));
  payment_address->postal_code =
      base::UTF16ToUTF8(profile->GetRawInfo(autofill::ADDRESS_HOME_ZIP));
  payment_address->sorting_code = base::UTF16ToUTF8(
      profile->GetRawInfo(autofill::ADDRESS_HOME_SORTING_CODE));
  payment_address->language_code = profile->language_code();
  payment_address->organization =
      base::UTF16ToUTF8(profile->GetRawInfo(autofill::COMPANY_NAME));
  payment_address->recipient = base::UTF16ToUTF8(profile->GetInfo(
      autofill::AutofillType(autofill::NAME_FULL), app_locale));

  // TODO(crbug.com/705945): Format phone number according to spec.
  payment_address->phone =
      base::UTF16ToUTF8(profile->GetRawInfo(autofill::PHONE_HOME_WHOLE_NUMBER));

  return payment_address;
}

void PaymentResponseHelper::OnInstrumentDetailsReady(
    const std::string& method_name,
    const std::string& stringified_details) {
  mojom::PaymentResponsePtr payment_response = mojom::PaymentResponse::New();

  // Make sure that we return the method name that the merchant specified for
  // this instrument: cards can be either specified through their name (e.g.,
  // "visa") or through basic-card's supportedNetworks.
  payment_response->method_name =
      spec_->IsMethodSupportedThroughBasicCard(method_name)
          ? kBasicCardMethodName
          : method_name;
  payment_response->stringified_details = stringified_details;

  // Shipping Address section
  if (spec_->request_shipping()) {
    DCHECK(selected_shipping_profile_);
    payment_response->shipping_address =
        GetMojomPaymentAddressFromAutofillProfile(selected_shipping_profile_,
                                                  app_locale_);

    DCHECK(spec_->selected_shipping_option());
    payment_response->shipping_option = spec_->selected_shipping_option()->id;
  }

  // Contact Details section.
  if (spec_->request_payer_name()) {
    DCHECK(selected_contact_profile_);
    payment_response->payer_name =
        base::UTF16ToUTF8(selected_contact_profile_->GetInfo(
            autofill::AutofillType(autofill::NAME_FULL), app_locale_));
  }
  if (spec_->request_payer_email()) {
    DCHECK(selected_contact_profile_);
    payment_response->payer_email = base::UTF16ToUTF8(
        selected_contact_profile_->GetRawInfo(autofill::EMAIL_ADDRESS));
  }
  if (spec_->request_payer_phone()) {
    DCHECK(selected_contact_profile_);

    // Try to format the phone number to the E.164 format to send in the Payment
    // Response, as defined in the Payment Request spec. If it's not possible,
    // send the original. More info at:
    // https://w3c.github.io/browser-payment-api/#paymentrequest-updated-algorithm
    // TODO(sebsg): Move this code to a reusable location.
    const std::string original_number =
        base::UTF16ToUTF8(selected_contact_profile_->GetInfo(
            autofill::AutofillType(autofill::PHONE_HOME_WHOLE_NUMBER),
            app_locale_));
    i18n::phonenumbers::PhoneNumber parsed_number;
    PhoneNumberUtil* phone_number_util = PhoneNumberUtil::GetInstance();
    if (phone_number_util->Parse(original_number, "US", &parsed_number) ==
        ::i18n::phonenumbers::PhoneNumberUtil::NO_PARSING_ERROR) {
      std::string formatted_number;
      phone_number_util->Format(parsed_number,
                                PhoneNumberUtil::PhoneNumberFormat::E164,
                                &formatted_number);
      payment_response->payer_phone = formatted_number;
    } else {
      payment_response->payer_phone = original_number;
    }
  }

  delegate_->OnPaymentResponseReady(std::move(payment_response));
}

}  // namespace payments
