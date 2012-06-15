// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_MONITOR_MONITOR_CONTROLLER_H_
#define ASH_MONITOR_MONITOR_CONTROLLER_H_
#pragma once

#include <map>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/display_observer.h"
#include "ui/aura/monitor_manager.h"

namespace aura {
class Display;
class RootWindow;
}

namespace ash {
namespace internal {
class RootWindowController;

// MonitorController owns and maintains RootWindows for each attached
// display, keeping them in sync with display configuration changes.
// TODO(oshima): Rename MonitorXXX to DisplayXXX.
class MonitorController : public aura::DisplayObserver {
 public:
  // Layout options where the secondary monitor should be positioned.
  enum SecondaryDisplayLayout {
    TOP,
    RIGHT,
    BOTTOM,
    LEFT
  };

  MonitorController();
  virtual ~MonitorController();

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

  // aura::DisplayObserver overrides:
  virtual void OnDisplayBoundsChanged(
      const gfx::Display& display) OVERRIDE;
  virtual void OnDisplayAdded(const gfx::Display& display) OVERRIDE;
  virtual void OnDisplayRemoved(const gfx::Display& display) OVERRIDE;

  // Is extended desktop enabled?
  static bool IsExtendedDesktopEnabled();
  // Change the extended desktop mode. Used for testing.
  static void SetExtendedDesktopEnabled(bool enabled);

 private:
  std::map<int, aura::RootWindow*> root_windows_;

  SecondaryDisplayLayout secondary_display_layout_;

  DISALLOW_COPY_AND_ASSIGN(MonitorController);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_MONITOR_MONITOR_CONTROLLER_H_
