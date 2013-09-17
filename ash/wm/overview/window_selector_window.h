// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_WINDOW_SELECTOR_WINDOW_H_
#define ASH_WM_OVERVIEW_WINDOW_SELECTOR_WINDOW_H_

#include "base/compiler_specific.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/transform.h"

namespace aura {
class RootWindow;
class Window;
}

namespace ui {
class Layer;
}

namespace views {
class Widget;
}

namespace ash {

// Manages a window in the overview mode. This class transitions the window
// to the best fit within the available overview rectangle, copying it if the
// window is sent to another display and restores the window state on
// deletion.
class WindowSelectorWindow {
 public:
  explicit WindowSelectorWindow(aura::Window* window);
  virtual ~WindowSelectorWindow();

  aura::Window* window() { return window_; }
  const aura::Window* window() const { return window_; }

  // Returns true if this window selector window contains the |target|. This is
  // used to determine if an event targetted this window.
  bool Contains(const aura::Window* target) const;

  // Restores this window on exit rather than returning it to a minimized state
  // if it was minimized on entering overview mode.
  void RestoreWindowOnExit();

  // Informs the WindowSelectorWindow that the window being watched was
  // destroyed. This resets the internal window pointer to avoid calling
  // anything on the window at destruction time.
  void OnWindowDestroyed();

  // Applies a transform to the window to fit within |target_bounds| while
  // maintaining its aspect ratio.
  void TransformToFitBounds(aura::RootWindow* root_window,
                            const gfx::Rect& target_bounds);

  const gfx::Rect& bounds() { return fit_bounds_; }

 private:
  // A weak pointer to the real window in the overview.
  aura::Window* window_;

  // A copy of the window used to transition the window to another root.
  views::Widget* window_copy_;

  // A weak pointer to a deep copy of the window's layers.
  ui::Layer* layer_;

  // If true, the window was minimized and should be restored if the window
  // was not selected.
  bool minimized_;

  // The original transform of the window before entering overview mode.
  gfx::Transform original_transform_;

  // The bounds this window is fit to.
  gfx::Rect fit_bounds_;

  DISALLOW_COPY_AND_ASSIGN(WindowSelectorWindow);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_WINDOW_SELECTOR_WINDOW_H_
