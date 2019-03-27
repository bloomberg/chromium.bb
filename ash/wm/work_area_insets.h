// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORK_AREA_INSETS_H_
#define ASH_WM_WORK_AREA_INSETS_H_

#include "ash/ash_export.h"
#include "base/macros.h"

namespace aura {
class Window;
}

namespace ash {
class RootWindowController;

// Insets of the work area associated with this root window.
class ASH_EXPORT WorkAreaInsets {
 public:
  // Returns work area parameters associated with the given |window|.
  static WorkAreaInsets* ForWindow(const aura::Window* window);

  explicit WorkAreaInsets(RootWindowController* root_window_controller);
  ~WorkAreaInsets();

  // Returns cached height of the accessibility panel in DIPs for this root
  // window.
  int accessibility_panel_height() const { return accessibility_panel_height_; }

  // Returns cached height of the docked magnifier in DIPs for this root
  // window.
  int docked_magnifier_height() const { return docked_magnifier_height_; }

  // Sets height of the accessibility panel in DIPs for this root window.
  // Shell observers will be notified that accessibility insets changed.
  void SetDockedMagnifierHeight(int height);

  // Sets height of the docked magnifier in DIPs for this root window.
  // Shell observers will be notified that accessibility insets changed.
  void SetAccessibilityPanelHeight(int height);

 private:
  // RootWindowController associated with this work area.
  RootWindowController* const root_window_controller_ = nullptr;

  // Cached height of the docked magnifier in DIPs at the top of the screen.
  // It needs to be removed from the available work area.
  int docked_magnifier_height_ = 0;

  // Cached height of the accessibility panel in DIPs at the top of the
  // screen. It needs to be removed from the available work area.
  int accessibility_panel_height_ = 0;

  DISALLOW_COPY_AND_ASSIGN(WorkAreaInsets);
};

}  // namespace ash

#endif  // ASH_WM_WORK_AREA_INSETS_H_
