// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_UTILS_H_
#define ASH_WM_OVERVIEW_OVERVIEW_UTILS_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/wm/overview/overview_animation_type.h"
#include "ash/wm/window_transient_descendant_iterator.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/compositor/layer_type.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/transform.h"

namespace aura {
class Window;
}  // namespace aura

namespace views {
class Widget;
}  // namespace views

namespace ash {

// Returns true if |window| can cover available workspace.
bool CanCoverAvailableWorkspace(aura::Window* window);

// Fades |widget| to opacity one with the enter overview settings. Additionally
// place |widget| closer to the top of screen and slide it down if |slide| is
// true.
void FadeInWidgetAndMaybeSlideOnEnter(views::Widget* widget,
                                      OverviewAnimationType animation_type,
                                      bool slide);

// Fades |widget| to opacity zero with animation settings depending on
// |animation_type|. Used by several classes which need to be destroyed on
// exiting overview, but have some widgets which need to continue animating.
// |widget| is destroyed after finishing animation.
void FadeOutWidgetAndMaybeSlideOnExit(std::unique_ptr<views::Widget> widget,
                                      OverviewAnimationType animation_type);

// Iterates through all the windows in the transient tree associated with
// |window| that are visible.
wm::WindowTransientDescendantIteratorRange GetVisibleTransientTreeIterator(
    aura::Window* window);

// Calculates the bounds of the |transformed_window|. Those bounds are a union
// of all regular (normal and panel) windows in the |transformed_window|'s
// transient hierarchy. The returned Rect is in virtual screen coordinates. The
// returned bounds are adjusted to allow the original |transformed_window|'s
// header to be hidden if |top_inset| is not zero.
gfx::RectF GetTransformedBounds(aura::Window* transformed_window,
                                int top_inset);

// Returns the original target bounds of |window|. The bounds are a union of all
// regular (normal and panel) windows in the window's transient hierarchy.
gfx::RectF GetTargetBoundsInScreen(aura::Window* window);

// Applies the |transform| to |window| and all of its transient children. Note
// |transform| is the transform that is applied to |window| and needs to be
// adjusted for the transient child windows.
ASH_EXPORT void SetTransform(aura::Window* window,
                             const gfx::Transform& transform);

// Checks if we are currently in sliding up on the shelf to hide overview mode.
bool IsSlidingOutOverviewFromShelf();

// Maximize the window if it is snapped without animation.
void MaximizeIfSnapped(aura::Window* window);

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_UTILS_H_
