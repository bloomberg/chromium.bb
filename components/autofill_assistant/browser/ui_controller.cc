// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill_assistant/browser/ui_controller.h"

#include "components/autofill/core/browser/autofill_profile.h"
#include "components/autofill/core/browser/credit_card.h"

namespace autofill_assistant {

PaymentInformation::PaymentInformation() : succeed(false) {}

PaymentInformation::~PaymentInformation() = default;

}  // namespace autofill_assistant
