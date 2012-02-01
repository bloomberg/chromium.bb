// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SHELF_LAYOUT_MANAGER_H_
#define ASH_WM_SHELF_LAYOUT_MANAGER_H_
#pragma once

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/aura/layout_manager.h"
#include "ash/ash_export.h"
#include "ui/gfx/compositor/layer_animation_observer.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/rect.h"

namespace views {
class Widget;
}

namespace ash {
namespace internal {

// ShelfLayoutManager is the layout manager responsible for the launcher and
// status widgets. The launcher is given the total available width and told the
// width of the status area. This allows the launcher to draw the background and
// layout to the status area.
// To respond to bounds changes in the status area StatusAreaLayoutManager works
// closely with ShelfLayoutManager.
class ASH_EXPORT ShelfLayoutManager : public aura::LayoutManager,
                                      public ui::LayerAnimationObserver {
 public:
  ShelfLayoutManager(views::Widget* launcher, views::Widget* status);
  virtual ~ShelfLayoutManager();

  bool in_layout() const { return in_layout_; }

  // Stops any animations and sets the bounds of the launcher and status
  // widgets.
  void LayoutShelf();

  // Sets the visibility of the shelf to |visible|.
  void SetVisible(bool visible);
  bool visible() const { return animating_ ? !visible_ : visible_; }

  views::Widget* launcher() { return launcher_; }
  views::Widget* status() { return status_; }

  // See description above field.
  int max_height() const { return max_height_; }

  // Overridden from aura::LayoutManager:
  virtual void OnWindowResized() OVERRIDE;
  virtual void OnWindowAddedToLayout(aura::Window* child) OVERRIDE;
  virtual void OnWillRemoveWindowFromLayout(aura::Window* child) OVERRIDE;
  virtual void OnChildWindowVisibilityChanged(aura::Window* child,
                                              bool visible) OVERRIDE;
  virtual void SetChildBounds(aura::Window* child,
                              const gfx::Rect& requested_bounds) OVERRIDE;

 private:
  struct TargetBounds {
    gfx::Rect launcher_bounds;
    gfx::Rect status_bounds;
    gfx::Insets work_area_insets;
  };

  // Stops any animations.
  void StopAnimating();

  // Calculates the target bounds assuming visibility of |visible|.
  void CalculateTargetBounds(bool visible,
                             TargetBounds* target_bounds);

  // Animates |widget| to the specified bounds and opacity.
  void AnimateWidgetTo(views::Widget* widget,
                       const gfx::Rect& target_bounds,
                       float target_opacity);

  // LayerAnimationObserver overrides:
  virtual void OnLayerAnimationEnded(
      const ui::LayerAnimationSequence* sequence) OVERRIDE;
  virtual void OnLayerAnimationAborted(
      const ui::LayerAnimationSequence* sequence) OVERRIDE {}
  virtual void OnLayerAnimationScheduled(
      const ui::LayerAnimationSequence* sequence) OVERRIDE {}

  // Are we animating?
  bool animating_;

  // True when inside LayoutShelf method. Used to prevent calling LayoutShelf
  // again from SetChildBounds().
  bool in_layout_;

  // Current visibility. When the visibility changes this field is reset once
  // the animation completes.
  bool visible_;

  // Max height needed.
  int max_height_;

  views::Widget* launcher_;
  views::Widget* status_;

  DISALLOW_COPY_AND_ASSIGN(ShelfLayoutManager);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_SHELF_LAYOUT_MANAGER_H_
