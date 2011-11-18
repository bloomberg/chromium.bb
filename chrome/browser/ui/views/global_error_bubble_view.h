// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_ERROR_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_ERROR_BUBBLE_VIEW_H_
#pragma once

#include "ui/views/bubble/bubble_delegate.h"
#include "views/controls/button/button.h"

class Browser;
class GlobalError;

class GlobalErrorBubbleView : public views::ButtonListener,
                              public views::BubbleDelegateView {
 public:
  GlobalErrorBubbleView(views::View* anchor_view,
                        views::BubbleBorder::ArrowLocation location,
                        const SkColor& color,
                        Browser* browser,
                        GlobalError* error);
  virtual ~GlobalErrorBubbleView();

  // views::BubbleDelegateView implementation.
  virtual gfx::Point GetAnchorPoint() OVERRIDE;

  // views::ButtonListener implementation.
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // views::WidgetDelegate implementation.
  virtual void WindowClosing() OVERRIDE;

 private:
  Browser* browser_;
  GlobalError* error_;

  DISALLOW_COPY_AND_ASSIGN(GlobalErrorBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_ERROR_BUBBLE_VIEW_H_
