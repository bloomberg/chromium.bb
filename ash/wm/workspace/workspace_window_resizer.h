// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_WINDOW_RESIZER_H_
#define ASH_WM_WORKSPACE_WINDOW_RESIZER_H_
#pragma once

#include "ash/wm/window_resizer.h"
#include "base/compiler_specific.h"

namespace ash {
namespace internal {

// WindowResizer implementation for workspaces. This enforces that windows are
// not allowed to vertically move or resize outside of the work area. As windows
// are moved outside the work area they are shrunk. We remember the height of
// the window before it was moved so that if the window is again moved up we
// attempt to restore the old height.
class ASH_EXPORT WorkspaceWindowResizer : public WindowResizer {
 public:
  WorkspaceWindowResizer(aura::Window* window,
                         const gfx::Point& location,
                         int window_component,
                         int grid_size);
  virtual ~WorkspaceWindowResizer();

 protected:
  // WindowResizer overrides:
  virtual gfx::Rect GetBoundsForDrag(const gfx::Point& location) OVERRIDE;
  virtual gfx::Rect GetFinalBounds() OVERRIDE;

 private:
  // Used to maintain the height of the window before we started collapsing it.
  static void SetHeightBeforeObscured(aura::Window* window, int height);
  static void ClearHeightBeforeObscured(aura::Window* window);
  static int GetHeightBeforeObscured(aura::Window* window);

  // Adjusts the bounds to enforce that windows are vertically contained in the
  // work area.
  void AdjustBounds(gfx::Rect* bounds) const;

  // Returns true if the window touches the bottom of the work area.
  bool WindowTouchesBottomOfScreen() const;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceWindowResizer);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_WORKSPACE_WINDOW_RESIZER_H_
