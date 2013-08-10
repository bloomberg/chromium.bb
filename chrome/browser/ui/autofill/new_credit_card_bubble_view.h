// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_NEW_CREDIT_CARD_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_AUTOFILL_NEW_CREDIT_CARD_BUBBLE_VIEW_H_

#include "base/memory/weak_ptr.h"

namespace autofill {

class NewCreditCardBubbleController;

// A cross-platform interface for a bubble that is shown when a new credit card
// is saved. Points at the settings menu in the toolbar and hides when
// activation is lost.
class NewCreditCardBubbleView {
 public:
  // The preferred width of the bubble's contents.
  static const int kContentsWidth;

  virtual ~NewCreditCardBubbleView();

  // Visually reveals the bubble.
  virtual void Show() = 0;

  // Hides the bubble from view.
  virtual void Hide() = 0;

  // Creates a bubble that's operated by |controller| and owns itself.
  // |controller| may be invalid while the bubble is closing.
  static base::WeakPtr<NewCreditCardBubbleView> Create(
      NewCreditCardBubbleController* controller);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_NEW_CREDIT_CARD_BUBBLE_VIEW_H_
