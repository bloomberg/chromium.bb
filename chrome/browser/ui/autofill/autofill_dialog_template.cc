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

const DetailInput kCCInputs[] = {
  { ++row_id, CREDIT_CARD_NUMBER, "Card number" },
  { ++row_id, CREDIT_CARD_EXP_2_DIGIT_YEAR, "Expiration MM/YY" },
  {   row_id, CREDIT_CARD_VERIFICATION_CODE, "CVC" },
  { ++row_id, CREDIT_CARD_NAME, "Cardholder name" },
};

const size_t kCCInputsSize = arraysize(kCCInputs);

const DetailInput kBillingInputs[] = {
  { ++row_id, ADDRESS_HOME_LINE1, "Street address", "billing" },
  { ++row_id, ADDRESS_HOME_LINE2, "Street address (optional)", "billing" },
  { ++row_id, ADDRESS_HOME_CITY, "City", "billing" },
  { ++row_id, ADDRESS_HOME_STATE, "State", "billing" },
  {   row_id, ADDRESS_HOME_ZIP, "ZIP code", "billing", 0.5 },
};

const size_t kBillingInputsSize = arraysize(kBillingInputs);

const DetailInput kShippingInputs[] = {
  { ++row_id, NAME_FULL, "Full name", "shipping" },
  { ++row_id, ADDRESS_HOME_LINE1, "Street address", "shipping" },
  { ++row_id, ADDRESS_HOME_LINE2, "Street address (optional)", "shipping" },
  { ++row_id, ADDRESS_HOME_CITY, "City", "shipping" },
  { ++row_id, ADDRESS_HOME_STATE, "State", "shipping" },
  {   row_id, ADDRESS_HOME_ZIP, "ZIP code", "shipping", 0.5 },
};

const size_t kShippingInputsSize = arraysize(kShippingInputs);

}  // namespace
