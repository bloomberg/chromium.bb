// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_SCREEN_ASH_H_
#define ASH_DISPLAY_SCREEN_ASH_H_

#include <stdint.h>

#include "ash/ash_export.h"
#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "ui/display/display_observer.h"
#include "ui/display/screen.h"

namespace gfx {
class Rect;
}

namespace ash {
namespace  internal {
class DisplayManager;
}

// Aura implementation of display::Screen. Implemented here to avoid circular
// dependencies.
class ASH_EXPORT ScreenAsh : public display::Screen {
 public:
  ScreenAsh();
  ~ScreenAsh() override;

  // display::Screen overrides:
  gfx::Point GetCursorScreenPoint() override;
  gfx::NativeWindow GetWindowUnderCursor() override;
  gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point) override;
  int GetNumDisplays() const override;
  std::vector<display::Display> GetAllDisplays() const override;
  display::Display GetDisplayNearestWindow(gfx::NativeView view) const override;
  display::Display GetDisplayNearestPoint(
      const gfx::Point& point) const override;
  display::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const override;
  display::Display GetPrimaryDisplay() const override;
  void AddObserver(display::DisplayObserver* observer) override;
  void RemoveObserver(display::DisplayObserver* observer) override;

 private:
  friend class DisplayManager;

  // Notifies observers of display configuration changes.
  void NotifyMetricsChanged(const display::Display& display, uint32_t metrics);
  void NotifyDisplayAdded(const display::Display& display);
  void NotifyDisplayRemoved(const display::Display& display);

  // Creates a screen that can be used during shutdown.
  // It simply has a copy of the displays.
  display::Screen* CloneForShutdown();

  base::ObserverList<display::DisplayObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(ScreenAsh);
};

}  // namespace ash

#endif  // ASH_DISPLAY_SCREEN_ASH_H_
