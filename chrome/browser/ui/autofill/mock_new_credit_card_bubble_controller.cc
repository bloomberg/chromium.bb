// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/mock_new_credit_card_bubble_controller.h"

#include "base/logging.h"
#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"

namespace autofill {

MockNewCreditCardBubbleController::MockNewCreditCardBubbleController()
    : bubbles_shown_(0) {}

MockNewCreditCardBubbleController::~MockNewCreditCardBubbleController() {}

void MockNewCreditCardBubbleController::Show(
    scoped_ptr<CreditCard> new_card,
    scoped_ptr<AutofillProfile> billing_profile) {
  CHECK(new_card);
  CHECK(billing_profile);

  new_card_ = new_card.Pass();
  billing_profile_ = billing_profile.Pass();

  ++bubbles_shown_;
}

}  // namespace autofill
