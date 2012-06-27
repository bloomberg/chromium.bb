// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_CONTROLLER_H_
#define ASH_DISPLAY_DISPLAY_CONTROLLER_H_
#pragma once

#include <map>
#include <vector>

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/display_observer.h"
#include "ui/aura/display_manager.h"

namespace aura {
class Display;
class RootWindow;
}

namespace ash {
namespace internal {
class RootWindowController;

// DisplayController owns and maintains RootWindows for each attached
// display, keeping them in sync with display configuration changes.
// TODO(oshima): Rename DisplayXXX to DisplayXXX.
class ASH_EXPORT DisplayController : public aura::DisplayObserver {
 public:
  // Layout options where the secondary display should be positioned.
  enum SecondaryDisplayLayout {
    TOP,
    RIGHT,
    BOTTOM,
    LEFT
  };

  DisplayController();
  virtual ~DisplayController();

  // Initializes primary display.
  void InitPrimaryDisplay();

  // Initialize secondary display. This is separated because in non
  // extended desktop mode, this creates background widgets, which
  // requires other controllers.
  void InitSecondaryDisplays();

  // Returns the root window for primary display.
  aura::RootWindow* GetPrimaryRootWindow();

  // Closes all child windows in the all root windows.
  void CloseChildWindows();

  // Returns all root windows. In non extended desktop mode, this
  // returns the primary root window only.
  std::vector<aura::RootWindow*> GetAllRootWindows();

  // Returns all oot window controllers. In non extended desktop
  // mode, this return a RootWindowController for the primary root window only.
  std::vector<internal::RootWindowController*> GetAllRootWindowControllers();

  SecondaryDisplayLayout secondary_display_layout() const {
    return secondary_display_layout_;
  }
  void SetSecondaryDisplayLayout(SecondaryDisplayLayout layout);

  // Warps the mouse cursor to an alternate root window when the
  // |location_in_root|, which is the location of the mouse cursor,
  // hits or exceeds the edge of the |root_window| and the mouse cursor
  // is considered to be in an alternate display. Returns true if
  // the cursor was moved.
  bool WarpMouseCursorIfNecessary(aura::Window* root_window,
                                  const gfx::Point& location_in_root);

  // aura::DisplayObserver overrides:
  virtual void OnDisplayBoundsChanged(
      const gfx::Display& display) OVERRIDE;
  virtual void OnDisplayAdded(const gfx::Display& display) OVERRIDE;
  virtual void OnDisplayRemoved(const gfx::Display& display) OVERRIDE;

  // Is extended desktop enabled?
  static bool IsExtendedDesktopEnabled();
  // Change the extended desktop mode. Used for testing.
  static void SetExtendedDesktopEnabled(bool enabled);

  // Is virtual screen coordinates enabled?
  static bool IsVirtualScreenCoordinatesEnabled();
  // Turns on/off the virtual screen coordinates.
  static void SetVirtualScreenCoordinatesEnabled(bool enabled);

 private:
  // Creates a root window for |display| and stores it in the |root_windows_|
  // map.
  aura::RootWindow* AddRootWindowForDisplay(const gfx::Display& display);

  void UpdateDisplayBoundsForLayout();

  std::map<int, aura::RootWindow*> root_windows_;

  SecondaryDisplayLayout secondary_display_layout_;

  DISALLOW_COPY_AND_ASSIGN(DisplayController);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_DISPLAY_DISPLAY_CONTROLLER_H_
