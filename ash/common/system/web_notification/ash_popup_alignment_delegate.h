// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_WEB_NOTIFICATION_ASH_POPUP_ALIGNMENT_DELEGATE_H_
#define ASH_COMMON_SYSTEM_WEB_NOTIFICATION_ASH_POPUP_ALIGNMENT_DELEGATE_H_

#include <stdint.h>

#include "ash/ash_export.h"
#include "ash/common/shelf/wm_shelf_observer.h"
#include "ash/common/shell_observer.h"
#include "ash/public/cpp/shelf_types.h"
#include "base/macros.h"
#include "ui/display/display_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/message_center/views/popup_alignment_delegate.h"

namespace display {
class Screen;
}

namespace ash {

class AshPopupAlignmentDelegateTest;
class WebNotificationTrayTest;
class WmShelf;

// The PopupAlignmentDelegate subclass for Ash. It needs to handle alignment of
// the shelf and its autohide state.
class ASH_EXPORT AshPopupAlignmentDelegate
    : public message_center::PopupAlignmentDelegate,
      public WmShelfObserver,
      public ShellObserver,
      public display::DisplayObserver {
 public:
  explicit AshPopupAlignmentDelegate(WmShelf* shelf);
  ~AshPopupAlignmentDelegate() override;

  // Start observing the system.
  void StartObserving(display::Screen* screen, const display::Display& display);

  // Sets the current height of the system tray bubble (or legacy notification
  // bubble) so that web notification toasts can avoid it.
  void SetTrayBubbleHeight(int height);

  // Returns the current tray bubble height or 0 if there is no bubble.
  int tray_bubble_height_for_test() const { return tray_bubble_height_; }

  // Overridden from message_center::PopupAlignmentDelegate:
  int GetToastOriginX(const gfx::Rect& toast_bounds) const override;
  int GetBaseLine() const override;
  gfx::Rect GetWorkArea() const override;
  bool IsTopDown() const override;
  bool IsFromLeft() const override;
  void RecomputeAlignment(const display::Display& display) override;
  void ConfigureWidgetInitParamsForContainer(
      views::Widget* widget,
      views::Widget::InitParams* init_params) override;

 private:
  friend class AshPopupAlignmentDelegateTest;
  friend class WebNotificationTrayTest;

  // Get the current alignment of the shelf.
  ShelfAlignment GetAlignment() const;

  // Utility function to get the display which should be care about.
  display::Display GetCurrentDisplay() const;

  // Compute the new work area.
  void UpdateWorkArea();

  // WmShelfObserver:
  void WillChangeVisibilityState(ShelfVisibilityState new_state) override;
  void OnAutoHideStateChanged(ShelfAutoHideState new_state) override;

  // Overridden from display::DisplayObserver:
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplayRemoved(const display::Display& old_display) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

  display::Screen* screen_;
  gfx::Rect work_area_;
  WmShelf* shelf_;
  int tray_bubble_height_;

  DISALLOW_COPY_AND_ASSIGN(AshPopupAlignmentDelegate);
};

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_WEB_NOTIFICATION_ASH_POPUP_ALIGNMENT_DELEGATE_H_
