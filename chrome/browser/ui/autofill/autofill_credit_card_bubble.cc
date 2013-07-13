// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_credit_card_bubble.h"

namespace autofill {

// static
const int AutofillCreditCardBubble::kContentWidth = 300;

AutofillCreditCardBubble::~AutofillCreditCardBubble() {}

#if !defined(TOOLKIT_VIEWS)
// static
base::WeakPtr<AutofillCreditCardBubble> AutofillCreditCardBubble::Create(
    const base::WeakPtr<AutofillCreditCardBubbleController>& controller) {
  // TODO(dbeam): make a bubble on all applicable platforms.
  return base::WeakPtr<AutofillCreditCardBubble>();
}
#endif

}  // namespace autofill
