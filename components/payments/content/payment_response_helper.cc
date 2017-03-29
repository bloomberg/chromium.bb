// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/payments/content/payment_response_helper.h"

#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/autofill_type.h"

namespace payments {

PaymentResponseHelper::PaymentResponseHelper(){};
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

}  // namespace payments
