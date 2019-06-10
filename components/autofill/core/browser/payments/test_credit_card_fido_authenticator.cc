// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/browser/payments/test_credit_card_fido_authenticator.h"

#include "base/strings/string16.h"
#include "components/autofill/core/browser/autofill_client.h"

namespace autofill {

TestCreditCardFIDOAuthenticator::TestCreditCardFIDOAuthenticator(
    AutofillClient* client)
    : CreditCardFIDOAuthenticator(client) {}

TestCreditCardFIDOAuthenticator::~TestCreditCardFIDOAuthenticator() {}

bool TestCreditCardFIDOAuthenticator::IsUserVerifiable() {
  return is_user_verifiable_;
}

bool TestCreditCardFIDOAuthenticator::IsUserOptedIn() {
  return is_user_opted_in_;
}

}  // namespace autofill
