// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/offers_private_api.h"

#include "base/logging.h"
#include "base/values.h"

ConfirmOfferEligibilityFunction::ConfirmOfferEligibilityFunction() {
}

ConfirmOfferEligibilityFunction::~ConfirmOfferEligibilityFunction() {
}

bool ConfirmOfferEligibilityFunction::RunImpl() {
  result_.reset(Value::CreateBooleanValue(true));
  return true;
}
