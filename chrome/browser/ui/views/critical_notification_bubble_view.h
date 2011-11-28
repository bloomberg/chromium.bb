// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CRITICAL_NOTIFICATION_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_CRITICAL_NOTIFICATION_BUBBLE_VIEW_H_
#pragma once

#include "base/timer.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/button/button.h"

namespace ui {
class Accelerator;
}

namespace views {
class Label;
class NativeTextButton;
}

class CriticalNotificationBubbleView : public views::BubbleDelegateView,
                                       public views::ButtonListener {
 public:
  explicit CriticalNotificationBubbleView(views::View* anchor_view);
  virtual ~CriticalNotificationBubbleView();

  // views::ButtonListener overrides:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // views::WidgetDelegate overrides:
  virtual void WindowClosing() OVERRIDE;

 protected:
  // views::BubbleDelegateView overrides:
  virtual bool AcceleratorPressed(
      const ui::Accelerator& accelerator) OVERRIDE;
  virtual void Init() OVERRIDE;

 private:
  // Helper function to calculate the remaining time (in seconds) until
  // spontaneous reboot.
  int GetRemainingTime();

  // Helper function to set the headline for the bubble.
  void UpdateBubbleHeadline(int seconds);

  // Called when the timer fires each time the clock ticks.
  void OnCountdown();

  // The headline and buttons on the bubble.
  views::Label* headline_;
  views::NativeTextButton* restart_button_;
  views::NativeTextButton* dismiss_button_;

  // A timer to refresh the bubble to show new countdown value.
  base::RepeatingTimer<CriticalNotificationBubbleView> refresh_timer_;

  // When the bubble was created.
  base::Time bubble_created_;

  DISALLOW_COPY_AND_ASSIGN(CriticalNotificationBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CRITICAL_NOTIFICATION_BUBBLE_VIEW_H_
