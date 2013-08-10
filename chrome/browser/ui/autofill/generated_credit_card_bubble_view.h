// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_GENERATED_CREDIT_CARD_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_AUTOFILL_GENERATED_CREDIT_CARD_BUBBLE_VIEW_H_

#include "base/memory/weak_ptr.h"

namespace autofill {

class GeneratedCreditCardBubbleController;

// A cross-platform interface for a bubble shown to educate users after a credit
// card number is generated. Anchored to an omnibox icon and shown either on
// creation or when the anchor is clicked. Hidden when focus is lost.
class GeneratedCreditCardBubbleView {
 public:
  // The preferred size of the bubble's contents.
  static const int kContentsWidth;

  virtual ~GeneratedCreditCardBubbleView();

  // Visually reveals the bubble.
  virtual void Show() = 0;

  // Hides the bubble from view.
  virtual void Hide() = 0;

  // Returns whether the bubble is currently in the process of hiding itself.
  virtual bool IsHiding() const = 0;

  // Creates a bubble that's operated by |controller| and owns itself.
  // |controller| may be invalid while the bubble is closing.
  static base::WeakPtr<GeneratedCreditCardBubbleView> Create(
      const base::WeakPtr<GeneratedCreditCardBubbleController>& controller);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_GENERATED_CREDIT_CARD_BUBBLE_VIEW_H_
