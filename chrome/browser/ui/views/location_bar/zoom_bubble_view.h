// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ZOOM_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ZOOM_BUBBLE_VIEW_H_
#pragma once

#include "base/basictypes.h"
#include "base/timer.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/controls/label.h"

// View used to display the zoom percentage when it has changed.
class ZoomBubbleView : public views::BubbleDelegateView {
 public:
  // Shows the bubble and automatically closes it after a short time period if
  // |auto_close| is true.
  static void ShowBubble(views::View* anchor_view,
                         int zoom_percent,
                         bool auto_close);
  static void CloseBubble();
  static bool IsShowing();

 private:
  ZoomBubbleView(views::View* anchor_view,
                 int zoom_percent,
                 bool auto_close);
  virtual ~ZoomBubbleView();

  void Close();

  // views::BubbleDelegateView method.
  virtual void Init() OVERRIDE;

  // views::BubbleDelegateView:
  virtual void WindowClosing() OVERRIDE;

  // Singleton instance of the zoom bubble. The zoom bubble can only be shown on
  // the active browser window, so there is no case in which it will be shown
  // twice at the same time.
  static ZoomBubbleView* zoom_bubble_;

  // Timer used to close the bubble when |auto_close_| is true.
  base::OneShotTimer<ZoomBubbleView> timer_;

  // Label displaying the zoom percentage.
  views::Label* label_;

  // The zoom percentage to show in the bubble.
  int zoom_percent_;

  // Whether the currently displayed bubble will automatically close.
  bool auto_close_;

  DISALLOW_COPY_AND_ASSIGN(ZoomBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_ZOOM_BUBBLE_VIEW_H_
