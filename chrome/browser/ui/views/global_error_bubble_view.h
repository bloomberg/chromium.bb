// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_ERROR_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_ERROR_BUBBLE_VIEW_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/global_error/global_error_bubble_view_base.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/button.h"

class Browser;
class GlobalErrorWithStandardBubble;

class GlobalErrorBubbleView : public views::ButtonListener,
                              public views::BubbleDelegateView,
                              public GlobalErrorBubbleViewBase {
 public:
  GlobalErrorBubbleView(
      views::View* anchor_view,
      views::BubbleBorder::Arrow arrow,
      Browser* browser,
      const base::WeakPtr<GlobalErrorWithStandardBubble>& error);
  virtual ~GlobalErrorBubbleView();

  // views::ButtonListener implementation.
  virtual void ButtonPressed(views::Button* sender,
                             const ui::Event& event) OVERRIDE;

  // views::WidgetDelegate implementation.
  virtual void WindowClosing() OVERRIDE;

  // GlobalErrorBubbleViewBase implementation.
  virtual void CloseBubbleView() OVERRIDE;

 private:
  Browser* browser_;
  base::WeakPtr<GlobalErrorWithStandardBubble> error_;

  DISALLOW_COPY_AND_ASSIGN(GlobalErrorBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_ERROR_BUBBLE_VIEW_H_
