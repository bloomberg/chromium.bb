// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_WEB_NOTIFICATION_ASH_POPUP_ALIGNMENT_DELEGATE_H_
#define ASH_SYSTEM_WEB_NOTIFICATION_ASH_POPUP_ALIGNMENT_DELEGATE_H_

#include <stdint.h>

#include "ash/ash_export.h"
#include "ash/shelf/shelf_layout_manager_observer.h"
#include "ash/shelf/shelf_types.h"
#include "ash/shell_observer.h"
#include "base/macros.h"
#include "ui/display/display_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/message_center/views/popup_alignment_delegate.h"

namespace aura {
class Window;
}

namespace gfx {
class Screen;
}

namespace display {
using Screen = gfx::Screen;
}

namespace ash {

class AshPopupAlignmentDelegateTest;
class ShelfLayoutManager;
class WebNotificationTrayTest;

// The PopupAlignmentDelegate subclass for Ash. It needs to handle alignment of
// the shelf and its autohide state.
class ASH_EXPORT AshPopupAlignmentDelegate
    : public message_center::PopupAlignmentDelegate,
      public ShelfLayoutManagerObserver,
      public ShellObserver,
      public display::DisplayObserver {
 public:
  explicit AshPopupAlignmentDelegate(ShelfLayoutManager* shelf);
  ~AshPopupAlignmentDelegate() override;

  // Start observing the system.
  void StartObserving(display::Screen* screen, const display::Display& display);

  // Sets the current height of the system tray so that the notification toasts
  // can avoid it.
  void SetSystemTrayHeight(int height);

  // Overridden from message_center::PopupAlignmentDelegate:
  int GetToastOriginX(const gfx::Rect& toast_bounds) const override;
  int GetBaseLine() const override;
  int GetWorkAreaBottom() const override;
  bool IsTopDown() const override;
  bool IsFromLeft() const override;
  void RecomputeAlignment(const display::Display& display) override;

 private:
  friend class AshPopupAlignmentDelegateTest;
  friend class WebNotificationTrayTest;

  // Get the current alignment of the shelf.
  wm::ShelfAlignment GetAlignment() const;

  // Utility function to get the display which should be care about.
  display::Display GetCurrentDisplay() const;

  // Compute the new work area.
  void UpdateWorkArea();

  // Overridden from ShellObserver:
  void OnDisplayWorkAreaInsetsChanged() override;

  // Overridden from ShelfLayoutManagerObserver:
  void WillChangeVisibilityState(ShelfVisibilityState new_state) override;
  void OnAutoHideStateChanged(ShelfAutoHideState new_state) override;

  // Overridden from display::DisplayObserver:
  void OnDisplayAdded(const display::Display& new_display) override;
  void OnDisplayRemoved(const display::Display& old_display) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

  display::Screen* screen_;
  gfx::Rect work_area_;
  ShelfLayoutManager* shelf_;
  int system_tray_height_;

  DISALLOW_COPY_AND_ASSIGN(AshPopupAlignmentDelegate);
};

}  // namespace ash

#endif  // ASH_SYSTEM_WEB_NOTIFICATION_ASH_POPUP_ALIGNMENT_DELEGATE_H_
