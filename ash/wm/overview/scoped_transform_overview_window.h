// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_SCOPED_TRANSFORM_OVERVIEW_WINDOW_H_
#define ASH_WM_OVERVIEW_SCOPED_TRANSFORM_OVERVIEW_WINDOW_H_

#include "ash/wm/overview/scoped_overview_animation_settings.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "ui/gfx/geometry/rect.h"
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

// Manages a window, and it's transient children, in the overview mode. This
// class allows transforming the windows with a helper to determine the best
// fit in certain bounds. The window's state is restored on destruction of this
// object.
class ScopedTransformOverviewWindow {
 public:
  typedef ScopedVector<ScopedOverviewAnimationSettings> ScopedAnimationSettings;

  // Returns |rect| having been shrunk to fit within |bounds| (preserving the
  // aspect ratio).
  static gfx::Rect ShrinkRectToFitPreservingAspectRatio(
      const gfx::Rect& rect,
      const gfx::Rect& bounds);

  // Returns the transform turning |src_rect| into |dst_rect|.
  static gfx::Transform GetTransformForRect(const gfx::Rect& src_rect,
                                            const gfx::Rect& dst_rect);

  explicit ScopedTransformOverviewWindow(aura::Window* window);
  ~ScopedTransformOverviewWindow();

  gfx::Transform get_overview_transform() const { return overview_transform_; }

  void set_overview_transform(const gfx::Transform& transform) {
    overview_transform_ = transform;
  }

  // Starts an animation sequence which will use animation settings specified by
  // |animation_type|. The |animation_settings| container is populated with
  // scoped entities and the container should be destroyed at the end of the
  // animation sequence.
  //
  // Example:
  //  ScopedTransformOverviewWindow overview_window(window);
  //  ScopedTransformOverviewWindow::ScopedAnimationSettings scoped_settings;
  //  overview_window.BeginScopedAnimation(
  //      OverviewAnimationType::OVERVIEW_ANIMATION_SELECTOR_ITEM_SCROLL_CANCEL,
  //      &animation_settings);
  //  // Calls to SetTransform & SetOpacity will use the same animation settings
  //  // until scoped_settings is destroyed.
  //  overview_window.SetTransform(root_window, new_transform);
  //  overview_window.SetOpacity(1);
  void BeginScopedAnimation(
      OverviewAnimationType animation_type,
      ScopedAnimationSettings* animation_settings);

  // Returns true if this window selector window contains the |target|.
  bool Contains(const aura::Window* target) const;

  // Returns the original target bounds of all transformed windows.
  gfx::Rect GetTargetBoundsInScreen() const;

  // Restores and animates the managed window to it's non overview mode state.
  void RestoreWindow();

  // Forces the managed window to be shown (ie not hidden or minimized) when
  // calling RestoreWindow().
  void ShowWindowOnExit();

  // Informs the ScopedTransformOverviewWindow that the window being watched was
  // destroyed. This resets the internal window pointer.
  void OnWindowDestroyed();

  // Prepares for overview mode by doing any necessary actions before entering.
  void PrepareForOverview();

  // Applies the |transform| to the overview window and all of its transient
  // children.
  void SetTransform(aura::Window* root_window,
                    const gfx::Transform& transform);

  // Set's the opacity of the managed windows.
  void SetOpacity(float opacity);

  aura::Window* window() const { return window_; }

  // Closes the transient root of the window managed by |this|.
  void Close();

 private:
  // Shows the window if it was minimized.
  void ShowWindowIfMinimized();

  // A weak pointer to the real window in the overview.
  aura::Window* window_;

  // If true, the window was minimized and should be restored if the window
  // was not selected.
  bool minimized_;

  // Tracks if this window was ignored by the shelf.
  bool ignored_by_shelf_;

  // True if the window has been transformed for overview mode.
  bool overview_started_;

  // The original transform of the window before entering overview mode.
  gfx::Transform original_transform_;

  // Keeps track of the original transform used when |this| has been positioned
  // during SelectorItem layout.
  gfx::Transform overview_transform_;

  // The original opacity of the window before entering overview mode.
  float original_opacity_;

  DISALLOW_COPY_AND_ASSIGN(ScopedTransformOverviewWindow);
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_SCOPED_TRANSFORM_OVERVIEW_WINDOW_H_
