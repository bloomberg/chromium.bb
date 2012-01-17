// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_GLOBAL_ERROR_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_GLOBAL_ERROR_BUBBLE_VIEW_H_
#pragma once

#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/button.h"

class Browser;
class GlobalError;

class GlobalErrorBubbleView : public views::ButtonListener,
                              public views::BubbleDelegateView,
                              public content::NotificationObserver {
 public:
  GlobalErrorBubbleView(views::View* anchor_view,
                        views::BubbleBorder::ArrowLocation location,
                        Browser* browser,
                        GlobalError* error);
  virtual ~GlobalErrorBubbleView();

  // views::BubbleDelegateView implementation.
  virtual gfx::Rect GetAnchorRect() OVERRIDE;

  // views::ButtonListener implementation.
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // views::WidgetDelegate implementation.
  virtual void WindowClosing() OVERRIDE;

  // content::NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  Browser* browser_;
  // Weak reference to the GlobalError instance the bubble is shown for. Is
  // reset to |NULL| if instance is removed from GlobalErrorService while
  // the bubble is still showing.
  GlobalError* error_;
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(GlobalErrorBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_GLOBAL_ERROR_BUBBLE_VIEW_H_
