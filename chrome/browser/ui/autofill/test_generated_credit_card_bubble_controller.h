// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_TEST_GENERATED_CREDIT_CARD_BUBBLE_CONTROLLER_H_
#define CHROME_BROWSER_UI_AUTOFILL_TEST_GENERATED_CREDIT_CARD_BUBBLE_CONTROLLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "chrome/browser/ui/autofill/generated_credit_card_bubble_controller.h"

namespace autofill {

class TestGeneratedCreditCardBubbleView;

class TestGeneratedCreditCardBubbleController
    : public GeneratedCreditCardBubbleController {
 public:
  explicit TestGeneratedCreditCardBubbleController(
      content::WebContents* contents);
  virtual ~TestGeneratedCreditCardBubbleController();

  // Whether this controller is installed on |web_contents()|.
  bool IsInstalled() const;

  // Get an invisible, dumb, test-only bubble.
  TestGeneratedCreditCardBubbleView* GetTestingBubble();

  // Made public for testing.
  using GeneratedCreditCardBubbleController::fronting_card_name;
  using GeneratedCreditCardBubbleController::backing_card_name;

  int bubbles_shown() const { return bubbles_shown_; }

 protected:
  // GeneratedCreditCardBubbleController:
  virtual base::WeakPtr<GeneratedCreditCardBubbleView> CreateBubble() OVERRIDE;
  virtual bool CanShow() const OVERRIDE;
  virtual void SetupAndShow(const base::string16& fronting_card_name,
                            const base::string16& backing_card_name) OVERRIDE;

 private:
  // How many bubbles have been shown via this controller.
  int bubbles_shown_;

  DISALLOW_COPY_AND_ASSIGN(TestGeneratedCreditCardBubbleController);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_TEST_GENERATED_CREDIT_CARD_BUBBLE_CONTROLLER_H_
