// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_BROWSER_CARD_UNMASK_DELEGATE_H_
#define COMPONENTS_AUTOFILL_CORE_BROWSER_CARD_UNMASK_DELEGATE_H_

#include "base/strings/string16.h"

namespace autofill {

class CardUnmaskDelegate {
 public:
  // Called when the user has attempted a verification. Prompt is still
  // open at this point.
  virtual void OnUnmaskResponse(const base::string16& cvc,
                                const base::string16& exp_month,
                                const base::string16& exp_year) = 0;

  // Called when the unmask prompt is closed (e.g., cancelled).
  virtual void OnUnmaskPromptClosed() = 0;
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_BROWSER_CARD_UNMASK_DELEGATE_H_
