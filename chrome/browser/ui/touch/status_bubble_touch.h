// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TOUCH_STATUS_BUBBLE_TOUCH_H_
#define CHROME_BROWSER_UI_TOUCH_STATUS_BUBBLE_TOUCH_H_
#pragma once

#include "chrome/browser/ui/views/status_bubble_views.h"
#include "ui/views/widget/widget.h"

// The touch version of the status bubble. This repositions itself as the
// keyboard slides up/down.
class StatusBubbleTouch : public StatusBubbleViews,
                          public views::Widget::Observer {
 public:
  explicit StatusBubbleTouch(views::View* base_view);
  virtual ~StatusBubbleTouch();

 private:
  // Overridden from StatusBubbleViews.
  virtual void Reposition() OVERRIDE;

  // Overridden from views::Widget::Observer.
  virtual void OnWidgetVisibilityChanged(views::Widget* widget,
                                         bool visible) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(StatusBubbleTouch);
};

#endif  // CHROME_BROWSER_UI_TOUCH_STATUS_BUBBLE_TOUCH_H_
