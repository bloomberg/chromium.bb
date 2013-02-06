// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOCHECKOUT_BUBBLE_VIEWS_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOCHECKOUT_BUBBLE_VIEWS_H_

#include "base/callback_forward.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/button.h"

namespace views {
class LabelButton;
}

namespace autofill {

class AutocheckoutBubbleController;

// A bubble for prompting a user to use Autocheckout to fill forms for them.
// The bubble is only displayed when the Autofill server hints that the current
// page is the start of an Autocheckout flow.
class AutocheckoutBubbleViews : public views::BubbleDelegateView,
                                public views::ButtonListener {
 public:
  // |bounding_box| is the anchor for the bubble UI. It is the bounds of an
  // input element in viewport space. |callback| is invoked if the bubble is
  // accepted. It brings up the requestAutocomplete dialog to collect user input
  // for Autocheckout.
  AutocheckoutBubbleViews(const gfx::RectF& bounding_box,
                          const base::Closure& callback);

 private:
  virtual ~AutocheckoutBubbleViews();

  // views::BubbleDelegateView:
  virtual void Init() OVERRIDE;
  virtual gfx::Rect GetAnchorRect() OVERRIDE;

  // views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  views::LabelButton* ok_button_;  // weak
  views::LabelButton* cancel_button_;  // weak

  // |bounding_box_| is the anchor for the bubble UI. It is the bounds of an
  // input element in viewport space.
  gfx::RectF bounding_box_;

  // |callback_| is invoked if the bubble is accepted.
  base::Closure callback_;

  DISALLOW_COPY_AND_ASSIGN(AutocheckoutBubbleViews);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_AUTOCHECKOUT_BUBBLE_VIEWS_H_
