// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_CREDIT_CARD_BUBBLE_H_
#define CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_CREDIT_CARD_BUBBLE_H_

#include "base/memory/weak_ptr.h"

namespace autofill {

class AutofillCreditCardBubbleController;

////////////////////////////////////////////////////////////////////////////////
//
// AutofillCreditCardBubble
//
//  An interface to be implemented on each platform to show a bubble after the
//  Autofill dialog is sucessfully submitted. Shows anchored to an icon that
//  looks like a credit card or the Google Wallet logo in the omnibox. Hidden
//  when focus is lost.
//
////////////////////////////////////////////////////////////////////////////////
class AutofillCreditCardBubble {
 public:
  // The preferred size of the bubble's contents.
  static const int kContentWidth;

  virtual ~AutofillCreditCardBubble();

  // Visually reveals the dialog bubble.
  virtual void Show() = 0;

  // Hides the bubble from view.
  virtual void Hide() = 0;

  // Returns whether the bubble is currently in the process of hiding itself.
  virtual bool IsHiding() const = 0;

  // Creates a bubble view that's operated by |controller| and owned by the
  // platform's widget or view management framework. |controller| is invalid
  // while the bubble is closing.
  static base::WeakPtr<AutofillCreditCardBubble> Create(
      const base::WeakPtr<AutofillCreditCardBubbleController>& controller);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_AUTOFILL_CREDIT_CARD_BUBBLE_H_
