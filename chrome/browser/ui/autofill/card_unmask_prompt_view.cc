// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "chrome/browser/ui/autofill/card_unmask_prompt_view.h"

namespace autofill {

#if !defined(OS_ANDROID) && !defined(TOOLKIT_VIEWS)
// static
CardUnmaskPromptView* CardUnmaskPromptView::CreateAndShow(
    CardUnmaskPromptController* controller) {
  return nullptr;
}
#endif

CardUnmaskPromptView::CardUnmaskPromptView() {
}
CardUnmaskPromptView::~CardUnmaskPromptView() {
}

}  // namespace autofill
