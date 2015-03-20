// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/card_unmask_prompt_view_tester_views.h"

#include "chrome/browser/ui/views/autofill/card_unmask_prompt_views.h"

namespace autofill {

// static
scoped_ptr<CardUnmaskPromptViewTester> CardUnmaskPromptViewTester::For(
    CardUnmaskPromptView* view) {
  return scoped_ptr<CardUnmaskPromptViewTester>(
      new CardUnmaskPromptViewTesterViews(
          static_cast<CardUnmaskPromptViews*>(view)));
}

// Class that facilitates testing.
CardUnmaskPromptViewTesterViews::CardUnmaskPromptViewTesterViews(
    CardUnmaskPromptViews* view)
    : view_(view) {
}

CardUnmaskPromptViewTesterViews::~CardUnmaskPromptViewTesterViews() {
}

void CardUnmaskPromptViewTesterViews::Close() {
  view_->ClosePrompt();
}

}  // namespace autofill
