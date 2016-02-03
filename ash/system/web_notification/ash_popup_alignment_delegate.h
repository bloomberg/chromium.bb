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
#include "ui/gfx/display_observer.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/message_center/views/popup_alignment_delegate.h"

namespace aura {
class Window;
}

namespace gfx {
class Screen;
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
      public gfx::DisplayObserver {
 public:
  explicit AshPopupAlignmentDelegate(ShelfLayoutManager* shelf);
  ~AshPopupAlignmentDelegate() override;

  // Start observing the system.
  void StartObserving(gfx::Screen* screen, const gfx::Display& display);

  // Sets the current height of the system tray so that the notification toasts
  // can avoid it.
  void SetSystemTrayHeight(int height);

  // Overridden from message_center::PopupAlignmentDelegate:
  int GetToastOriginX(const gfx::Rect& toast_bounds) const override;
  int GetBaseLine() const override;
  int GetWorkAreaBottom() const override;
  bool IsTopDown() const override;
  bool IsFromLeft() const override;
  void RecomputeAlignment(const gfx::Display& display) override;

 private:
  friend class AshPopupAlignmentDelegateTest;
  friend class WebNotificationTrayTest;

  // Get the current alignment of the shelf.
  ShelfAlignment GetAlignment() const;

  // Utility function to get the display which should be care about.
  gfx::Display GetCurrentDisplay() const;

  // Compute the new work area.
  void UpdateWorkArea();

  // Overridden from ShellObserver:
  void OnDisplayWorkAreaInsetsChanged() override;

  // Overridden from ShelfLayoutManagerObserver:
  void WillChangeVisibilityState(ShelfVisibilityState new_state) override;
  void OnAutoHideStateChanged(ShelfAutoHideState new_state) override;

  // Overridden from gfx::DisplayObserver:
  void OnDisplayAdded(const gfx::Display& new_display) override;
  void OnDisplayRemoved(const gfx::Display& old_display) override;
  void OnDisplayMetricsChanged(const gfx::Display& display,
                               uint32_t metrics) override;

  gfx::Screen* screen_;
  gfx::Rect work_area_;
  aura::Window* root_window_;
  ShelfLayoutManager* shelf_;
  int system_tray_height_;

  DISALLOW_COPY_AND_ASSIGN(AshPopupAlignmentDelegate);
};

}  // namespace ash

#endif  // ASH_SYSTEM_WEB_NOTIFICATION_ASH_POPUP_ALIGNMENT_DELEGATE_H_
