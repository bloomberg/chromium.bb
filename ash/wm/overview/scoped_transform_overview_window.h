// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_SCOPED_TRANSFORM_OVERVIEW_WINDOW_H_
#define ASH_WM_OVERVIEW_SCOPED_TRANSFORM_OVERVIEW_WINDOW_H_

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

// Manages a window in the overview mode. This class allows transforming the
// window with a helper to determine the best fit in certain bounds and
// copies the window if being moved to another display. The window's state is
// restored on destruction of this object.
class ScopedTransformOverviewWindow {
 public:
  // The duration of transitions used for window transforms.
  static const int kTransitionMilliseconds;

  // Returns the transform necessary to fit |rect| into |bounds| preserving
  // aspect ratio and centering.
  static gfx::Transform GetTransformForRectPreservingAspectRatio(
      const gfx::Rect& rect,
      const gfx::Rect& bounds);

  explicit ScopedTransformOverviewWindow(aura::Window* window);
  virtual ~ScopedTransformOverviewWindow();

  // Returns true if this window selector window contains the |target|. This is
  // used to determine if an event targetted this window.
  bool Contains(const aura::Window* target) const;

  // Restores the window if it was minimized.
  void RestoreWindow();

  // Restores this window on exit rather than returning it to a minimized state
  // if it was minimized on entering overview mode.
  void RestoreWindowOnExit();

  // Informs the ScopedTransformOverviewWindow that the window being watched was
  // destroyed. This resets the internal window pointer to avoid calling
  // anything on the window at destruction time.
  void OnWindowDestroyed();

  // Sets |transform| on the window and a copy of the window if the target
  // |root_window| is not the window's root window.
  void SetTransform(aura::RootWindow* root_window,
                    const gfx::Transform& transform);

  aura::Window* window() const { return window_; }

 protected:
  // Dispatched when the overview of this window has started.
  virtual void OnOverviewStarted();

 private:
  // Applies the |transform| to the overview window and all of its transient
  // children using animations.
  void AnimateTransformOnWindowAndTransientChildren(
      const gfx::Transform& transform);

  // A weak pointer to the real window in the overview.
  aura::Window* window_;

  // A copy of the window used to transition the window to another root.
  views::Widget* window_copy_;

  // A weak pointer to a deep copy of the window's layers.
  ui::Layer* layer_;

  // If true, the window was minimized and should be restored if the window
  // was not selected.
  bool minimized_;

  // True if the window has been transformed for overview mode.
  bool overview_started_;

  // The original transform of the window before entering overview mode.
  gfx::Transform original_transform_;

  DISALLOW_COPY_AND_ASSIGN(ScopedTransformOverviewWindow);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_SCOPED_TRANSFORM_OVERVIEW_WINDOW_H_
