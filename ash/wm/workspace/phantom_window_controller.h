// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_PHANTOM_WINDOW_CONTROLLER_H_
#define ASH_WM_WORKSPACE_PHANTOM_WINDOW_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer.h"
#include "ui/gfx/rect.h"

namespace aura {
class Window;
}

namespace views {
class Widget;
}

namespace ash {
namespace internal {

// PhantomWindowController is responsible for showing a phantom beneath an
// existing window. PhantomWindowController is used during dragging a window to
// give an indication where on a grid the window will land.
class ASH_EXPORT PhantomWindowController {
 public:
  // |delay_ms| specifies the delay before the phantom is shown.
  PhantomWindowController(aura::Window* window, int delay_ms);
  ~PhantomWindowController();

  // Shows the phantom window at the specified location (coordinates of the
  // parent). This does not immediately show the window.
  void Show(const gfx::Rect& bounds);

  // Hides the phantom.
  void Hide();

  // Closes the phantom window after a delay (in milliseconds).
  void DelayedClose(int delay_ms);

 private:
  // Shows the window immediately.
  void ShowNow();

  // Window the phantom is placed beneath.
  aura::Window* window_;

  // Delay before closing.
  const int delay_ms_;

  // Last bounds passed to Show().
  gfx::Rect bounds_;

  scoped_ptr<views::Widget> phantom_widget_;

  // Timer used to show the phantom window.
  base::OneShotTimer<PhantomWindowController> show_timer_;

  DISALLOW_COPY_AND_ASSIGN(PhantomWindowController);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_WORKSPACE_PHANTOM_WINDOW_CONTROLLER_H_
