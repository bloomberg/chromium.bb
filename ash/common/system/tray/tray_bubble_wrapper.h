// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_TRAY_TRAY_BUBBLE_WRAPPER_H_
#define ASH_COMMON_SYSTEM_TRAY_TRAY_BUBBLE_WRAPPER_H_

#include "base/macros.h"
#include "ui/views/widget/widget_observer.h"

namespace views {
class TrayBubbleView;
}

namespace ash {
class TrayBackgroundView;

// Creates and manages the Widget and EventFilter components of a bubble.

class TrayBubbleWrapper : public views::WidgetObserver {
 public:
  TrayBubbleWrapper(TrayBackgroundView* tray,
                    views::TrayBubbleView* bubble_view);
  ~TrayBubbleWrapper() override;

  // views::WidgetObserver overrides:
  void OnWidgetDestroying(views::Widget* widget) override;
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;

  const TrayBackgroundView* tray() const { return tray_; }
  TrayBackgroundView* tray() { return tray_; }
  views::TrayBubbleView* bubble_view() { return bubble_view_; }
  const views::TrayBubbleView* bubble_view() const { return bubble_view_; }
  views::Widget* bubble_widget() { return bubble_widget_; }
  const views::Widget* bubble_widget() const { return bubble_widget_; }

 private:
  TrayBackgroundView* tray_;
  views::TrayBubbleView* bubble_view_;  // unowned
  views::Widget* bubble_widget_;

  DISALLOW_COPY_AND_ASSIGN(TrayBubbleWrapper);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_TRAY_TRAY_BUBBLE_WRAPPER_H_
