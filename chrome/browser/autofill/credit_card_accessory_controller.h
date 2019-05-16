// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_CREDIT_CARD_ACCESSORY_CONTROLLER_H_
#define CHROME_BROWSER_AUTOFILL_CREDIT_CARD_ACCESSORY_CONTROLLER_H_

#include "chrome/browser/autofill/accessory_controller.h"

namespace autofill {

// Interface for credit card-specific keyboard accessory controller between the
// ManualFillingController and Autofill backend logic.
class CreditCardAccessoryController : public AccessoryController {
 public:
  CreditCardAccessoryController() = default;
  ~CreditCardAccessoryController() override = default;

  // Fetches suggestions and propagates to the frontend.
  virtual void RefreshSuggestionsForField() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_CREDIT_CARD_ACCESSORY_CONTROLLER_H_
