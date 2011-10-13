// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_CRITICAL_NOTIFICATION_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_CRITICAL_NOTIFICATION_BUBBLE_VIEW_H_
#pragma once

#include "base/timer.h"
#include "chrome/browser/ui/views/bubble/bubble.h"
#include "views/controls/button/button.h"
#include "views/view.h"

namespace views {
class ImageView;
class Label;
class NativeTextButton;
}

class CriticalNotificationBubbleView : public views::View,
                                       public views::ButtonListener,
                                       public BubbleDelegate,
                                       public ui::AnimationDelegate {
 public:
  CriticalNotificationBubbleView();
  virtual ~CriticalNotificationBubbleView();

  void set_bubble(Bubble* bubble) { bubble_ = bubble; }

  // views::View methods:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;

  // views::ButtonListener methods:
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // BubbleDelegate methods:
  virtual void BubbleClosing(Bubble* bubble, bool closed_by_escape) OVERRIDE;
  virtual bool CloseOnEscape() OVERRIDE;
  virtual bool FadeInOnShow() OVERRIDE;
  virtual string16 GetAccessibleName() OVERRIDE;

 private:
  // Helper function to calculate the remaining time (in seconds) until
  // spontaneous reboot.
  int GetRemainingTime();

  // Helper function to set the headline for the bubble.
  void UpdateBubbleHeadline(int seconds);

  // Called when the timer fires each time the clock ticks.
  void OnCountdown();

  // Global pointer to the bubble that is hosting our view.
  static Bubble* bubble_;

  // The views on the bubble.
  views::ImageView* image_;
  views::Label* headline_;
  views::Label* message_;
  views::NativeTextButton* restart_button_;
  views::NativeTextButton* dismiss_button_;

  // A timer to refresh the bubble to show new countdown value.
  base::RepeatingTimer<CriticalNotificationBubbleView> refresh_timer_;

  // When the bubble was created.
  base::Time bubble_created_;

  DISALLOW_COPY_AND_ASSIGN(CriticalNotificationBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_CRITICAL_NOTIFICATION_BUBBLE_VIEW_H_
