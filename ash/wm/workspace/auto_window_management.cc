// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/auto_window_management.h"

#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "ash/wm/property_util.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_util.h"
#include "base/command_line.h"
#include "ui/aura/window.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/compositor/layer_animator.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/screen.h"

namespace ash {
namespace internal {

namespace {

// The time in milliseconds which should be used to visually move a window
// through an automatic "intelligent" window management option.
const int kWindowAutoMoveDurationMS = 125;

// Check if any management should be performed (with a given |window|).
bool UseAutoWindowMagerForWindow(const aura::Window* window) {
  return !CommandLine::ForCurrentProcess()->HasSwitch(
             switches::kAshDisableAutoWindowPlacement) &&
         GetTrackedByWorkspace(window) &&
         wm::IsWindowPositionManaged(window);
}

// Check if a given |window| can be managed. This includes that it's state is
// not minimized/maximized/the user has changed it's size by hand already.
// It furthermore checks for the WindowIsManaged status.
bool WindowPositionCanBeManaged(const aura::Window* window) {
  return (wm::IsWindowPositionManaged(window) &&
          !wm::IsWindowMinimized(window) &&
          !wm::IsWindowMaximized(window) &&
          !wm::HasUserChangedWindowPositionOrSize(window));
}

// Given a |window|, return the only |other_window| which has an impact on
// the automated windows location management. If there is more then one window,
// false is returned, but the |other_window| will be set to the first one
// found.
// If the return value is true a single window was found.
bool GetOtherVisibleAndManageableWindow(const aura::Window* window,
                                        aura::Window** other_window) {
  *other_window = NULL;
  const aura::Window::Windows& windows = window->parent()->children();
  // Find a single open managed window.
  for (size_t i = 0; i < windows.size(); i++) {
    aura::Window* iterated_window = windows[i];
    if (window != iterated_window &&
        iterated_window->type() == aura::client::WINDOW_TYPE_NORMAL &&
        iterated_window->TargetVisibility() &&
        wm::IsWindowPositionManaged(iterated_window)) {
      // Bail if we find a second usable window.
      if (*other_window)
        return false;
      *other_window = iterated_window;
    }
  }
  return *other_window != NULL;
}

// Get the work area for a given |window|.
gfx::Rect GetWorkAreaForWindow(const aura::Window* window) {
  gfx::Rect work_area = gfx::Rect(window->parent()->bounds().size());
  work_area.Inset(Shell::GetScreen()->GetDisplayMatching(
      work_area).GetWorkAreaInsets());
  return work_area;
}

// Move the given |bounds| on the available |parent_width| to the
// direction. If |move_right| is true, the rectangle gets moved to the right
// corner, otherwise to the left one.
bool MoveRectToOneSide(int parent_width, bool move_right, gfx::Rect* bounds) {
  if (move_right) {
    if (parent_width > bounds->right()) {
      bounds->set_x(parent_width - bounds->width());
      return true;
    }
  } else {
    if (0 < bounds->x()) {
      bounds->set_x(0);
      return true;
    }
  }
  return false;
}

// Move a |window| to a new |bound|. Animate if desired by user.
// Note: The function will do nothing if the bounds did not change.
void SetBoundsAnimated(aura::Window* window, const gfx::Rect& bounds) {
  if (bounds == window->GetTargetBounds())
    return;

  if (views::corewm::WindowAnimationsDisabled(window)) {
    window->SetBounds(bounds);
    return;
  }

  ui::ScopedLayerAnimationSettings settings(window->layer()->GetAnimator());
  settings.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kWindowAutoMoveDurationMS));
  window->SetBounds(bounds);
}

// Move |window| into the center of the screen - or restore it to the previous
// position.
void AutoPlaceSingleWindow(aura::Window* window, bool animated) {
  gfx::Rect work_area = GetWorkAreaForWindow(window);
  gfx::Rect bounds = window->bounds();
  const gfx::Rect* user_defined_area =
      ash::wm::GetPreAutoManageWindowBounds(window);
  if (user_defined_area) {
    bounds = *user_defined_area;
    ash::wm::AdjustBoundsToEnsureMinimumWindowVisibility(work_area, &bounds);
  } else {
    // Center the window (only in x).
    bounds.set_x((work_area.width() - bounds.width()) / 2);
  }

  if (animated)
    SetBoundsAnimated(window, bounds);
  else
    window->SetBounds(bounds);
}

} // namespace

void RearrangeVisibleWindowOnHideOrRemove(const aura::Window* removed_window) {
  if (!UseAutoWindowMagerForWindow(removed_window))
    return;
  // Find a single open browser window.
  aura::Window* other_shown_window = NULL;
  if (!GetOtherVisibleAndManageableWindow(removed_window,
                                          &other_shown_window) ||
      !WindowPositionCanBeManaged(other_shown_window))
    return;
  AutoPlaceSingleWindow(other_shown_window, true);
}

void RearrangeVisibleWindowOnShow(aura::Window* added_window) {
  if (!UseAutoWindowMagerForWindow(added_window) ||
      wm::HasUserChangedWindowPositionOrSize(added_window) ||
      !added_window->TargetVisibility())
    return;
  // Find a single open managed window.
  aura::Window* other_shown_window = NULL;
  if (!GetOtherVisibleAndManageableWindow(added_window,
                                          &other_shown_window)) {
    // It could be that this window is the first window joining the workspace.
    if (!WindowPositionCanBeManaged(added_window) || other_shown_window)
      return;
    // Since we might be going from 0 to 1 window, we have to arrange the new
    // window to a good default.
    AutoPlaceSingleWindow(added_window, false);
    return;
  }

  // When going from one to two windows both windows loose their "positioned
  // by user" flags.
  ash::wm::SetUserHasChangedWindowPositionOrSize(added_window, false);
  ash::wm::SetUserHasChangedWindowPositionOrSize(other_shown_window, false);

  if (WindowPositionCanBeManaged(other_shown_window)) {
    gfx::Rect work_area = GetWorkAreaForWindow(added_window);

    // Push away the other window after remembering its current position.
    gfx::Rect other_bounds = other_shown_window->bounds();
    ash::wm::SetPreAutoManageWindowBounds(other_shown_window, other_bounds);

    bool move_right = other_bounds.CenterPoint().x() > work_area.width() / 2;
    if (MoveRectToOneSide(work_area.width(), move_right, &other_bounds))
      SetBoundsAnimated(other_shown_window, other_bounds);

    // Remember the current location of the new window and push it also to the
    // opposite location (if needed).
    // Since it is just coming into view, we do not need to animate it.
    gfx::Rect added_bounds = added_window->bounds();
    ash::wm::SetPreAutoManageWindowBounds(added_window, added_bounds);
    if (MoveRectToOneSide(work_area.width(), !move_right, &added_bounds))
      added_window->SetBounds(added_bounds);
  }
}

} // namespace internal
} // namespace ash
