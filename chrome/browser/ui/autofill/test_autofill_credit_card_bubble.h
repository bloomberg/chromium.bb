// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_TEST_AUTOFILL_CREDIT_CARD_BUBBLE_H_
#define CHROME_BROWSER_UI_AUTOFILL_TEST_AUTOFILL_CREDIT_CARD_BUBBLE_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_credit_card_bubble.h"

namespace autofill {

////////////////////////////////////////////////////////////////////////////////
//
// TestAutofillCreditCardBubble
//
//  A cross-platform, headless bubble that doesn't conflict with other top-level
//  widgets/windows.
////////////////////////////////////////////////////////////////////////////////
class TestAutofillCreditCardBubble : public AutofillCreditCardBubble {
 public:
  // Creates a bubble and returns a weak reference to it.
  static base::WeakPtr<TestAutofillCreditCardBubble> Create(
      const base::WeakPtr<AutofillCreditCardBubbleController>& controller);

  virtual ~TestAutofillCreditCardBubble();

  // AutofillCreditCardBubble:
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual bool IsHiding() const OVERRIDE;

  base::WeakPtr<TestAutofillCreditCardBubble> GetWeakPtr();

  bool showing() const { return showing_; }

 private:
  explicit TestAutofillCreditCardBubble(
      const base::WeakPtr<AutofillCreditCardBubbleController>& controller);

  // Whether the bubble is currently showing or not.
  bool showing_;

  base::WeakPtrFactory<TestAutofillCreditCardBubble> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(TestAutofillCreditCardBubble);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_TEST_AUTOFILL_CREDIT_CARD_BUBBLE_H_
