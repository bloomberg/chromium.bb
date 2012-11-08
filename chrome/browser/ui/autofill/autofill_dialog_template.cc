// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_dialog_template.h"

#include "base/basictypes.h"

namespace autofill {

static int row_id = 0;

const DetailInput kEmailInputs[] = {
  { ++row_id, EMAIL_ADDRESS, "Email address" },
};

const size_t kEmailInputsSize = arraysize(kEmailInputs);

const DetailInput kBillingInputs[] = {
  { ++row_id, CREDIT_CARD_NUMBER, "Card number" },
  { ++row_id, CREDIT_CARD_EXP_2_DIGIT_YEAR, "Expiration MM/YY" },
  {   row_id, CREDIT_CARD_VERIFICATION_CODE, "CVC" },
  { ++row_id, CREDIT_CARD_NAME, "Cardholder name" },
  { ++row_id, ADDRESS_BILLING_LINE1, "Street address" },
  { ++row_id, ADDRESS_BILLING_LINE2, "Street address (optional)" },
  { ++row_id, ADDRESS_BILLING_CITY, "City" },
  { ++row_id, ADDRESS_BILLING_STATE, "State" },
  {   row_id, ADDRESS_BILLING_ZIP, "ZIP code", 0.5 },
};

const size_t kBillingInputsSize = arraysize(kBillingInputs);

const DetailInput kShippingInputs[] = {
  { ++row_id, NAME_FULL, "Full name" },
  { ++row_id, ADDRESS_HOME_LINE1, "Street address" },
  { ++row_id, ADDRESS_HOME_LINE2, "Street address (optional)" },
  { ++row_id, ADDRESS_HOME_CITY, "City" },
  { ++row_id, ADDRESS_HOME_STATE, "State" },
  {   row_id, ADDRESS_HOME_ZIP, "ZIP code", 0.5 },
};

const size_t kShippingInputsSize = arraysize(kShippingInputs);

}  // namespace
