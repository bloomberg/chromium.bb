// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_GENERATED_CREDIT_CARD_BUBBLE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_GENERATED_CREDIT_CARD_BUBBLE_VIEWS_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/generated_credit_card_bubble_view.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/styled_label_listener.h"

namespace autofill {

class GeneratedCreditCardBubbleController;

// Views toolkit implementation of the GeneratedCreditCardBubbleView (an
// educational bubble shown after a successful generation of a new credit card
// number).
class GeneratedCreditCardBubbleViews : public GeneratedCreditCardBubbleView,
                                       public views::BubbleDelegateView,
                                       public views::StyledLabelListener {
 public:
  virtual ~GeneratedCreditCardBubbleViews();

  // GeneratedCreditCardBubbleView:
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual bool IsHiding() const OVERRIDE;

  // views::BubbleDelegateView:
  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  virtual base::string16 GetWindowTitle() const OVERRIDE;
  virtual void Init() OVERRIDE;

  // views::StyledLabelListener:
  virtual void StyledLabelLinkClicked(const gfx::Range& range, int event_flags)
      OVERRIDE;

 private:
  friend base::WeakPtr<GeneratedCreditCardBubbleView>
      GeneratedCreditCardBubbleView::Create(
          const base::WeakPtr<GeneratedCreditCardBubbleController>& controller);

  explicit GeneratedCreditCardBubbleViews(
     const base::WeakPtr<GeneratedCreditCardBubbleController>& controller);

  // Controller that drives this bubble. May be invalid when hiding.
  base::WeakPtr<GeneratedCreditCardBubbleController> controller_;

  base::WeakPtrFactory<GeneratedCreditCardBubbleViews> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(GeneratedCreditCardBubbleViews);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_GENERATED_CREDIT_CARD_BUBBLE_VIEWS_H_
