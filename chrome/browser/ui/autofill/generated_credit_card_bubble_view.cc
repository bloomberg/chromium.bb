// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/generated_credit_card_bubble_view.h"

namespace autofill {

// static
const int GeneratedCreditCardBubbleView::kContentsWidth = 350;

GeneratedCreditCardBubbleView::~GeneratedCreditCardBubbleView() {}

#if !defined(TOOLKIT_VIEWS) && !defined(OS_MACOSX)
// static
base::WeakPtr<GeneratedCreditCardBubbleView>
    GeneratedCreditCardBubbleView::Create(
        const base::WeakPtr<GeneratedCreditCardBubbleController>& controller) {
  // TODO(dbeam): make a bubble on all applicable platforms.
  return base::WeakPtr<GeneratedCreditCardBubbleView>();
}
#endif

}  // namespace autofill
