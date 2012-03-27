// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_PHANTOM_WINDOW_CONTROLLER_H_
#define ASH_WM_WORKSPACE_PHANTOM_WINDOW_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/rect.h"

namespace aura {
class Window;
}

namespace views {
class Widget;
}

namespace ash {
namespace internal {

// PhantomWindowController is responsible for showing a phantom above a window.
// It's used used during dragging a window to show a snap location.
class ASH_EXPORT PhantomWindowController {
 public:
  explicit PhantomWindowController(aura::Window* window);
  ~PhantomWindowController();

  // Bounds last passed to Show().
  const gfx::Rect& bounds() const { return bounds_; }

  // Shows the phantom window at the specified location (coordinates of the
  // parent). This does not immediately show the window.
  void Show(const gfx::Rect& bounds);

  // Hides the phantom.
  void Hide();

  // Returns true if the phantom is showing.
  bool IsShowing() const;

 private:
  // Shows the window immediately.
  void ShowNow();

  // Window the phantom is placed beneath.
  aura::Window* window_;

  // Last bounds passed to Show().
  gfx::Rect bounds_;

  scoped_ptr<views::Widget> phantom_widget_;

  DISALLOW_COPY_AND_ASSIGN(PhantomWindowController);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_WORKSPACE_PHANTOM_WINDOW_CONTROLLER_H_
