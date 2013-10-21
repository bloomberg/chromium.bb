// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/test_generated_credit_card_bubble_controller.h"

#include "chrome/browser/ui/autofill/test_generated_credit_card_bubble_view.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {

TestGeneratedCreditCardBubbleController::
    TestGeneratedCreditCardBubbleController(content::WebContents* contents)
    : GeneratedCreditCardBubbleController(contents),
      bubbles_shown_(0) {
  contents->SetUserData(UserDataKey(), this);
}

TestGeneratedCreditCardBubbleController::
    ~TestGeneratedCreditCardBubbleController() {}

bool TestGeneratedCreditCardBubbleController::IsInstalled() const {
  return web_contents()->GetUserData(UserDataKey()) == this;
}

TestGeneratedCreditCardBubbleView* TestGeneratedCreditCardBubbleController::
    GetTestingBubble() {
  return static_cast<TestGeneratedCreditCardBubbleView*>(
      GeneratedCreditCardBubbleController::bubble().get());
}

base::WeakPtr<GeneratedCreditCardBubbleView>
    TestGeneratedCreditCardBubbleController::CreateBubble() {
  return TestGeneratedCreditCardBubbleView::Create(GetWeakPtr());
}

bool TestGeneratedCreditCardBubbleController::CanShow() const {
  return true;
}

void TestGeneratedCreditCardBubbleController::SetupAndShow(
    const base::string16& fronting_card_name,
    const base::string16& backing_card_name) {
  GeneratedCreditCardBubbleController::SetupAndShow(fronting_card_name,
                                                    backing_card_name);
  ++bubbles_shown_;
}

}  // namespace autofill
