// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_NEW_CREDIT_CARD_BUBBLE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_NEW_CREDIT_CARD_BUBBLE_VIEWS_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/new_credit_card_bubble_view.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/link_listener.h"

namespace autofill {

class NewCreditCardBubbleController;

// Views toolkit implementation of NewCreditCardBubbleView (a bubble shown after
// a user saved a new card locally in Chrome).
class NewCreditCardBubbleViews : public NewCreditCardBubbleView,
                                 public views::BubbleDelegateView,
                                 public views::LinkListener {
 public:
  virtual ~NewCreditCardBubbleViews();

  // NewCreditCardBubbleView:
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;

  // views::BubbleDelegateView:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual base::string16 GetWindowTitle() const OVERRIDE;
  virtual void Init() OVERRIDE;
  virtual gfx::Rect GetBubbleBounds() OVERRIDE;

  // views::LinkListener:
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

 private:
  friend base::WeakPtr<NewCreditCardBubbleView> NewCreditCardBubbleView::Create(
      NewCreditCardBubbleController* controller);

  explicit NewCreditCardBubbleViews(
     NewCreditCardBubbleController* controller);

  // Controller that drives this bubble. Never NULL; outlives this class.
  NewCreditCardBubbleController* controller_;

  base::WeakPtrFactory<NewCreditCardBubbleView> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(NewCreditCardBubbleViews);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_NEW_CREDIT_CARD_BUBBLE_VIEWS_H_
