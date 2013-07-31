// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_CREDIT_CARD_BUBBLE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_CREDIT_CARD_BUBBLE_VIEWS_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_credit_card_bubble.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/link_listener.h"

namespace autofill {

class AutofillCreditCardBubbleController;

// Views toolkit implementation of AutofillCreditCard bubble (an educational
// bubble shown after a successful submission of the Autofill dialog).
class AutofillCreditCardBubbleViews : public AutofillCreditCardBubble,
                                      public views::BubbleDelegateView,
                                      public views::LinkListener {
 public:
  virtual ~AutofillCreditCardBubbleViews();

  // AutofillCreditCardBubble:
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual bool IsHiding() const OVERRIDE;

  // views::BubbleDelegateView:
  virtual string16 GetWindowTitle() const OVERRIDE;
  virtual void Init() OVERRIDE;
  virtual gfx::Size GetPreferredSize() OVERRIDE;

  // views::LinkListener:
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

 private:
  friend base::WeakPtr<AutofillCreditCardBubble>
      AutofillCreditCardBubble::Create(
          const base::WeakPtr<AutofillCreditCardBubbleController>& controller);

  explicit AutofillCreditCardBubbleViews(
     const base::WeakPtr<AutofillCreditCardBubbleController>& controller);

  // Controller that drives this bubble. Invalid when hiding.
  base::WeakPtr<AutofillCreditCardBubbleController> controller_;

  base::WeakPtrFactory<AutofillCreditCardBubble> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(AutofillCreditCardBubbleViews);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOFILL_CREDIT_CARD_BUBBLE_VIEWS_H_
