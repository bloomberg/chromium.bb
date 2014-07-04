// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_SCOPED_TRANSFORM_OVERVIEW_WINDOW_H_
#define ASH_WM_OVERVIEW_SCOPED_TRANSFORM_OVERVIEW_WINDOW_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/transform.h"
#include "ui/views/widget/widget.h"

namespace aura {
class Window;
}

namespace ui {
class Layer;
}

namespace views {
class Widget;
}

namespace ash {

class ScopedWindowCopy;

// Manages a window in the overview mode. This class allows transforming the
// window with a helper to determine the best fit in certain bounds and
// copies the window if being moved to another display. The window's state is
// restored on destruction of this object.
class ScopedTransformOverviewWindow {
 public:
  // The duration of transitions used for window transforms.
  static const int kTransitionMilliseconds;

  // Returns |rect| having been shrunk to fit within |bounds| (preserving the
  // aspect ratio).
  static gfx::Rect ShrinkRectToFitPreservingAspectRatio(
      const gfx::Rect& rect,
      const gfx::Rect& bounds);

  // Returns the transform turning |src_rect| into |dst_rect|.
  static gfx::Transform GetTransformForRect(const gfx::Rect& src_rect,
                                            const gfx::Rect& dst_rect);

  explicit ScopedTransformOverviewWindow(aura::Window* window);
  virtual ~ScopedTransformOverviewWindow();

  // Returns true if this window selector window contains the |target|. This is
  // used to determine if an event targeted this window.
  bool Contains(const aura::Window* target) const;

  // Returns the original bounds of all transformed windows.
  gfx::Rect GetBoundsInScreen() const;

  // Restores the window if it was minimized.
  void RestoreWindow();

  // Restores this window on exit rather than returning it to a minimized state
  // if it was minimized on entering overview mode.
  void RestoreWindowOnExit();

  // Informs the ScopedTransformOverviewWindow that the window being watched was
  // destroyed. This resets the internal window pointer to avoid calling
  // anything on the window at destruction time.
  void OnWindowDestroyed();

  // Prepares for overview mode by doing any necessary actions before entering.
  virtual void PrepareForOverview();

  // Sets |transform| on the window and a copy of the window if the target
  // |root_window| is not the window's root window. If |animate| the transform
  // is animated in, otherwise it is immediately applied.
  virtual void SetTransform(aura::Window* root_window,
                            const gfx::Transform& transform,
                            bool animate);

  aura::Window* window() const { return window_; }

 private:
  // Creates copies of |window| and all of its modal transient parents on the
  // root window |target_root|.
  void CopyWindowAndTransientParents(aura::Window* target_root,
                                     aura::Window* window);

  // Applies the |transform| to the overview window and all of its transient
  // children using animations. If |animate| the transform is animated in,
  // otherwise it is applied immediately.
  void SetTransformOnWindowAndTransientChildren(const gfx::Transform& transform,
                                                bool animate);

  // A weak pointer to the real window in the overview.
  aura::Window* window_;

  // Copies of the window and transient parents for a different root window.
  ScopedVector<ScopedWindowCopy> window_copies_;

  // If true, the window was minimized and should be restored if the window
  // was not selected.
  bool minimized_;

  // Tracks if this window was ignored by the shelf.
  bool ignored_by_shelf_;

  // True if the window has been transformed for overview mode.
  bool overview_started_;

  // The original transform of the window before entering overview mode.
  gfx::Transform original_transform_;

  // The original opacity of the window before entering overview mode.
  float opacity_;

  DISALLOW_COPY_AND_ASSIGN(ScopedTransformOverviewWindow);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_SCOPED_TRANSFORM_OVERVIEW_WINDOW_H_
