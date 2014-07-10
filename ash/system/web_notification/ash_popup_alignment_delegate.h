// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_WEB_NOTIFICATION_ASH_POPUP_ALIGNMENT_DELEGATE_H_
#define ASH_SYSTEM_WEB_NOTIFICATION_ASH_POPUP_ALIGNMENT_DELEGATE_H_

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
  AshPopupAlignmentDelegate();
  virtual ~AshPopupAlignmentDelegate();

  // Start observing the system.
  void StartObserving(gfx::Screen* screen, const gfx::Display& display);

  // Sets the current height of the system tray so that the notification toasts
  // can avoid it.
  void SetSystemTrayHeight(int height);

  // Overridden from message_center::PopupAlignmentDelegate:
  virtual int GetToastOriginX(const gfx::Rect& toast_bounds) const OVERRIDE;
  virtual int GetBaseLine() const OVERRIDE;
  virtual int GetWorkAreaBottom() const OVERRIDE;
  virtual bool IsTopDown() const OVERRIDE;
  virtual bool IsFromLeft() const OVERRIDE;
  virtual void RecomputeAlignment(const gfx::Display& display) OVERRIDE;

 private:
  friend class AshPopupAlignmentDelegateTest;
  friend class WebNotificationTrayTest;

  // Get the current alignment of the shelf.
  ShelfAlignment GetAlignment() const;

  // Update |shelf_| and start watching when it's first set. This should not
  // be done in the constructor because the shelf might not be initialized at
  // that point.
  void UpdateShelf();

  // Overridden from ShellObserver:
  virtual void OnDisplayWorkAreaInsetsChanged() OVERRIDE;

  // Overridden from ShelfLayoutManagerObserver:
  virtual void OnAutoHideStateChanged(ShelfAutoHideState new_state) OVERRIDE;

  // Overridden from gfx::DisplayObserver:
  virtual void OnDisplayAdded(const gfx::Display& new_display) OVERRIDE;
  virtual void OnDisplayRemoved(const gfx::Display& old_display) OVERRIDE;
  virtual void OnDisplayMetricsChanged(const gfx::Display& display,
                                       uint32_t metrics) OVERRIDE;

  int64_t display_id_;
  gfx::Screen* screen_;
  gfx::Rect work_area_;
  aura::Window* root_window_;
  ShelfLayoutManager* shelf_;
  int system_tray_height_;

  DISALLOW_COPY_AND_ASSIGN(AshPopupAlignmentDelegate);
};

}  // namespace ash

#endif  // ASH_SYSTEM_WEB_NOTIFICATION_ASH_POPUP_ALIGNMENT_DELEGATE_H_
