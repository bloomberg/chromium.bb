// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_TRAY_TRAY_BUBBLE_WRAPPER_H_
#define ASH_SYSTEM_TRAY_TRAY_BUBBLE_WRAPPER_H_

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/views/widget/widget_observer.h"
#include "ui/wm/public/activation_change_observer.h"

namespace views {
class TrayBubbleView;
}

namespace ash {
class TrayBackgroundView;

// Creates and manages the Widget and EventFilter components of a bubble.

class ASH_EXPORT TrayBubbleWrapper : public views::WidgetObserver,
                                     public ::wm::ActivationChangeObserver {
 public:
  TrayBubbleWrapper(TrayBackgroundView* tray,
                    views::TrayBubbleView* bubble_view,
                    bool is_persistent);
  ~TrayBubbleWrapper() override;

  // views::WidgetObserver overrides:
  void OnWidgetDestroying(views::Widget* widget) override;
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override;

  // ::wm::ActivationChangeObserver overrides:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

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
  bool is_persistent_;

  DISALLOW_COPY_AND_ASSIGN(TrayBubbleWrapper);
};

}  // namespace ash

#endif  // ASH_SYSTEM_TRAY_TRAY_BUBBLE_WRAPPER_H_
