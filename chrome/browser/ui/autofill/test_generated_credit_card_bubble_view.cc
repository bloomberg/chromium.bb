// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/test_generated_credit_card_bubble_view.h"

#include "chrome/browser/ui/autofill/generated_credit_card_bubble_controller.h"

namespace autofill {

// static
base::WeakPtr<TestGeneratedCreditCardBubbleView>
    TestGeneratedCreditCardBubbleView::Create(
        const base::WeakPtr<GeneratedCreditCardBubbleController>& controller) {
  return (new TestGeneratedCreditCardBubbleView(controller))->GetWeakPtr();
}

TestGeneratedCreditCardBubbleView::~TestGeneratedCreditCardBubbleView() {}

void TestGeneratedCreditCardBubbleView::Show() {
  showing_ = true;
}

void TestGeneratedCreditCardBubbleView::Hide() {
  delete this;
}

bool TestGeneratedCreditCardBubbleView::IsHiding() const {
  return !showing_;
}

base::WeakPtr<TestGeneratedCreditCardBubbleView>
    TestGeneratedCreditCardBubbleView::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

TestGeneratedCreditCardBubbleView::TestGeneratedCreditCardBubbleView(
    const base::WeakPtr<GeneratedCreditCardBubbleController>& controller)
    : controller_(controller),
      showing_(false),
      weak_ptr_factory_(this) {}

}  // namespace autofill
