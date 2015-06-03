// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_SCREEN_ASH_H_
#define ASH_DISPLAY_SCREEN_ASH_H_

#include "ash/ash_export.h"
#include "base/compiler_specific.h"
#include "base/observer_list.h"
#include "ui/gfx/display_observer.h"
#include "ui/gfx/screen.h"

namespace gfx {
class Rect;
}

namespace ash {
namespace  internal {
class DisplayManager;
}

// Aura implementation of gfx::Screen. Implemented here to avoid circular
// dependencies.
class ASH_EXPORT ScreenAsh : public gfx::Screen {
 public:
  ScreenAsh();
  ~ScreenAsh() override;

  // gfx::Screen overrides:
  gfx::Point GetCursorScreenPoint() override;
  gfx::NativeWindow GetWindowUnderCursor() override;
  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override;
  int GetNumDisplays() const override;
  std::vector<gfx::Display> GetAllDisplays() const override;
  gfx::Display GetDisplayNearestWindow(gfx::NativeView view) const override;
  gfx::Display GetDisplayNearestPoint(const gfx::Point& point) const override;
  gfx::Display GetDisplayMatching(const gfx::Rect& match_rect) const override;
  gfx::Display GetPrimaryDisplay() const override;
  void AddObserver(gfx::DisplayObserver* observer) override;
  void RemoveObserver(gfx::DisplayObserver* observer) override;

 private:
  friend class DisplayManager;

  // Notifies observers of display configuration changes.
  void NotifyMetricsChanged(const gfx::Display& display, uint32_t metrics);
  void NotifyDisplayAdded(const gfx::Display& display);
  void NotifyDisplayRemoved(const gfx::Display& display);

  // Creates a screen that can be used during shutdown.
  // It simply has a copy of the displays.
  gfx::Screen* CloneForShutdown();

  base::ObserverList<gfx::DisplayObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(ScreenAsh);
};

}  // namespace ash

#endif  // ASH_DISPLAY_SCREEN_ASH_H_
