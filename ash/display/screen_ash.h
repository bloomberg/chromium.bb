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
  virtual ~ScreenAsh();

  // Finds the display that contains |point| in screeen coordinates.
  // Returns invalid display if there is no display that can satisfy
  // the condition.
  static gfx::Display FindDisplayContainingPoint(const gfx::Point& point);

  // Returns the bounds for maximized windows in parent coordinates.
  // Maximized windows trigger auto-hiding the shelf.
  static gfx::Rect GetMaximizedWindowBoundsInParent(aura::Window* window);

  // Returns the display bounds in parent coordinates.
  static gfx::Rect GetDisplayBoundsInParent(aura::Window* window);

  // Returns the display's work area bounds in parent coordinates.
  static gfx::Rect GetDisplayWorkAreaBoundsInParent(aura::Window* window);

  // Converts |rect| from |window|'s coordinates to the virtual screen
  // coordinates.
  static gfx::Rect ConvertRectToScreen(aura::Window* window,
                                       const gfx::Rect& rect);

  // Converts |rect| from virtual screen coordinates to the |window|'s
  // coordinates.
  static gfx::Rect ConvertRectFromScreen(aura::Window* window,
                                         const gfx::Rect& rect);

  // Returns a gfx::Display object for secondary display. Returns
  // invalid display if there is no secondary display connected.
  static const gfx::Display& GetSecondaryDisplay();

  // Returns a gfx::Display object for the specified id.  Returns
  // invalid display if no such display is connected.
  static const gfx::Display& GetDisplayForId(int64 display_id);

  // gfx::Screen overrides:
  virtual bool IsDIPEnabled() OVERRIDE;
  virtual gfx::Point GetCursorScreenPoint() OVERRIDE;
  virtual gfx::NativeWindow GetWindowUnderCursor() OVERRIDE;
  virtual gfx::NativeWindow GetWindowAtScreenPoint(const gfx::Point& point)
      OVERRIDE;
  virtual int GetNumDisplays() const OVERRIDE;
  virtual std::vector<gfx::Display> GetAllDisplays() const OVERRIDE;
  virtual gfx::Display GetDisplayNearestWindow(
      gfx::NativeView view) const OVERRIDE;
  virtual gfx::Display GetDisplayNearestPoint(
      const gfx::Point& point) const OVERRIDE;
  virtual gfx::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const OVERRIDE;
  virtual gfx::Display GetPrimaryDisplay() const OVERRIDE;
  virtual void AddObserver(gfx::DisplayObserver* observer) OVERRIDE;
  virtual void RemoveObserver(gfx::DisplayObserver* observer) OVERRIDE;

 private:
  friend class DisplayManager;

  // Notifies observers of display configuration changes.
  void NotifyMetricsChanged(const gfx::Display& display, uint32_t metrics);
  void NotifyDisplayAdded(const gfx::Display& display);
  void NotifyDisplayRemoved(const gfx::Display& display);

  // Creates a screen that can be used during shutdown.
  // It simply has a copy of the displays.
  gfx::Screen* CloneForShutdown();

  ObserverList<gfx::DisplayObserver> observers_;

  DISALLOW_COPY_AND_ASSIGN(ScreenAsh);
};

}  // namespace ash

#endif  // ASH_DISPLAY_SCREEN_ASH_H_
