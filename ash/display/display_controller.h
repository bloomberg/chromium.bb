// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_CONTROLLER_H_
#define ASH_DISPLAY_DISPLAY_CONTROLLER_H_

#include <map>
#include <vector>

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
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

  // Initialize secondary displays.
  void InitSecondaryDisplays();

  // Returns the root window for primary display.
  aura::RootWindow* GetPrimaryRootWindow();

  // Returns the root window for |display_id|.
  aura::RootWindow* GetRootWindowForDisplayId(int64 id);

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

  int secondary_display_offset() const {
    return secondary_display_offset_;
  }
  void SetSecondaryDisplayOffset(int offset);

  void set_dont_warp_mouse(bool dont_warp_mouse) {
    dont_warp_mouse_ = dont_warp_mouse;
  }

  // Warps the mouse cursor to an alternate root window when the
  // |point_in_root|, which is the location of the mouse cursor,
  // hits or exceeds the edge of the |root_window| and the mouse cursor
  // is considered to be in an alternate display. Returns true if
  // the cursor was moved.
  bool WarpMouseCursorIfNecessary(aura::RootWindow* root_window,
                                  const gfx::Point& point_in_root);

  // aura::DisplayObserver overrides:
  virtual void OnDisplayBoundsChanged(
      const gfx::Display& display) OVERRIDE;
  virtual void OnDisplayAdded(const gfx::Display& display) OVERRIDE;
  virtual void OnDisplayRemoved(const gfx::Display& display) OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(WorkspaceWindowResizerTest, WarpMousePointer);

  // Creates a root window for |display| and stores it in the |root_windows_|
  // map.
  aura::RootWindow* AddRootWindowForDisplay(const gfx::Display& display);

  void UpdateDisplayBoundsForLayout();

  // The mapping from display ID to its root window.
  std::map<int64, aura::RootWindow*> root_windows_;

  SecondaryDisplayLayout secondary_display_layout_;

  // The offset of the position of the secondary display.  The offset is
  // based on the top/left edge of the primary display.
  int secondary_display_offset_;

  // If true, the mouse pointer can't move from one display to another.
  bool dont_warp_mouse_;

  DISALLOW_COPY_AND_ASSIGN(DisplayController);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_DISPLAY_DISPLAY_CONTROLLER_H_
