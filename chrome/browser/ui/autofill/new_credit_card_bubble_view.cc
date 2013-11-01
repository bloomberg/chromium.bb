// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/new_credit_card_bubble_view.h"

namespace autofill {

NewCreditCardBubbleView::~NewCreditCardBubbleView() {}

// static
const int NewCreditCardBubbleView::kContentsWidth = 400;

#if !defined(TOOLKIT_VIEWS) && !defined(OS_MACOSX)
// static
base::WeakPtr<NewCreditCardBubbleView> NewCreditCardBubbleView::Create(
    NewCreditCardBubbleController* controller) {
  // TODO(dbeam): make a bubble on all applicable platforms.
  return base::WeakPtr<NewCreditCardBubbleView>();
}
#endif

}  // namespace autofill
