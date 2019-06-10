// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/credit_card_fido_authenticator.h"

#include "base/strings/string16.h"
#include "components/autofill/core/browser/autofill_client.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/payments/payments_client.h"
#include "components/autofill/core/common/autofill_payments_features.h"

namespace autofill {

CreditCardFIDOAuthenticator::CreditCardFIDOAuthenticator(AutofillClient* client)
    : autofill_client_(client),
      payments_client_(client->GetPaymentsClient()),
      weak_ptr_factory_(this) {}

CreditCardFIDOAuthenticator::~CreditCardFIDOAuthenticator() {}

void CreditCardFIDOAuthenticator::Authenticate(
    const CreditCard* card,
    base::WeakPtr<Requester> requester,
    base::Value request_options) {
  // TODO(crbug/949269): Add call to InternalAuthenticatorImpl::GetAssertion.
  requester_ = requester;
  requester_->OnFIDOAuthenticationComplete(false);
}

bool CreditCardFIDOAuthenticator::IsUserVerifiable() {
  // TODO(crbug/949269): Add call to
  // InternalAuthenticatorImpl::IsUserVerifyingPlatformAuthenticatorAvailable.
  return base::FeatureList::IsEnabled(
             features::kAutofillCreditCardAuthentication) &&
         false;
}

bool CreditCardFIDOAuthenticator::IsUserOptedIn() {
  // TODO(crbug/949269): Check pref-store for user opt-in.
  return false;
}

}  // namespace autofill
