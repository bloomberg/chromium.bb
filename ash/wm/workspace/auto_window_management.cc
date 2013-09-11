// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/auto_window_management.h"

#include "ash/ash_switches.h"
#include "ash/shell.h"
#include "ash/wm/mru_window_tracker.h"
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
bool UseAutoWindowManager(const aura::Window* window) {
  return GetTrackedByWorkspace(window) &&
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

// Get the work area for a given |window|.
gfx::Rect GetWorkAreaForWindow(aura::Window* window) {
#if defined(OS_WIN)
  // On Win 8, the host window can't be resized, so
  // use window's bounds instead.
  // TODO(oshima): Emulate host window resize on win8.
  gfx::Rect work_area = gfx::Rect(window->parent()->bounds().size());
  work_area.Inset(Shell::GetScreen()->GetDisplayMatching(
      work_area).GetWorkAreaInsets());
  return work_area;
#else
  return Shell::GetScreen()->GetDisplayNearestWindow(window).work_area();
#endif
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

// Get the first open (non minimized) window which is on the screen defined.
aura::Window* GetReferenceWindow(const aura::RootWindow* root_window,
                                 const aura::Window* exclude,
                                 bool *single_window) {
  if (single_window)
    *single_window = true;
  // Get the active window.
  aura::Window* active = ash::wm::GetActiveWindow();
  if (active && active->GetRootWindow() != root_window)
    active = NULL;

  // Get a list of all windows.
  const std::vector<aura::Window*> windows =
      ash::MruWindowTracker::BuildWindowList(false);

  if (windows.empty())
    return NULL;

  aura::Window::Windows::const_iterator iter = windows.begin();
  // Find the index of the current active window.
  if (active)
    iter = std::find(windows.begin(), windows.end(), active);

  int index = (iter == windows.end()) ? 0 : (iter - windows.begin());

  // Scan the cycle list backwards to see which is the second topmost window
  // (and so on). Note that we might cycle a few indices twice if there is no
  // suitable window. However - since the list is fairly small this should be
  // very fast anyways.
  aura::Window* found = NULL;
  for (int i = index + windows.size(); i >= 0; i--) {
    aura::Window* window = windows[i % windows.size()];
    if (window != exclude &&
        window->type() == aura::client::WINDOW_TYPE_NORMAL &&
        window->GetRootWindow() == root_window &&
        window->TargetVisibility() &&
        wm::IsWindowPositionManaged(window)) {
      if (found && found != window) {
        // no need to check !signle_window because the function must have
        // been already returned in the "if (!single_window)" below.
        *single_window = false;
        return found;
      }
      found = window;
      // If there is no need to check single window, return now.
      if (!single_window)
        return found;
    }
  }
  return found;
}

} // namespace

void RearrangeVisibleWindowOnHideOrRemove(const aura::Window* removed_window) {
  if (!UseAutoWindowManager(removed_window))
    return;
  // Find a single open browser window.
  bool single_window;
  aura::Window* other_shown_window = GetReferenceWindow(
      removed_window->GetRootWindow(), removed_window, &single_window);
  if (!other_shown_window || !single_window ||
      !WindowPositionCanBeManaged(other_shown_window))
    return;
  AutoPlaceSingleWindow(other_shown_window, true);
}

void RearrangeVisibleWindowOnShow(aura::Window* added_window) {
  if (!UseAutoWindowManager(added_window) ||
      wm::HasUserChangedWindowPositionOrSize(added_window) ||
      !added_window->TargetVisibility())
    return;
  // Find a single open managed window.
  bool single_window;
  aura::Window* other_shown_window = GetReferenceWindow(
      added_window->GetRootWindow(), added_window, &single_window);

  if (!other_shown_window) {
    // It could be that this window is the first window joining the workspace.
    if (!WindowPositionCanBeManaged(added_window) || other_shown_window)
      return;
    // Since we might be going from 0 to 1 window, we have to arrange the new
    // window to a good default.
    AutoPlaceSingleWindow(added_window, false);
    return;
  }

  gfx::Rect other_bounds = other_shown_window->bounds();
  gfx::Rect work_area = GetWorkAreaForWindow(added_window);
  bool move_other_right =
      other_bounds.CenterPoint().x() > work_area.width() / 2;

  // Push the other window to the size only if there are two windows left.
  if (single_window) {
    // When going from one to two windows both windows loose their "positioned
    // by user" flags.
    ash::wm::SetUserHasChangedWindowPositionOrSize(added_window, false);
    ash::wm::SetUserHasChangedWindowPositionOrSize(other_shown_window, false);

    if (WindowPositionCanBeManaged(other_shown_window)) {
      // Don't override pre auto managed bounds as the current bounds
      // may not be original.
      if (!ash::wm::GetPreAutoManageWindowBounds(other_shown_window))
        ash::wm::SetPreAutoManageWindowBounds(other_shown_window, other_bounds);

      // Push away the other window after remembering its current position.
      if (MoveRectToOneSide(work_area.width(), move_other_right, &other_bounds))
        SetBoundsAnimated(other_shown_window, other_bounds);
    }
  }

  // Remember the current location of the window if it's new and push
  // it also to the opposite location if needed.  Since it is just
  // being shown, we do not need to animate it.
  gfx::Rect added_bounds = added_window->bounds();
  if (!ash::wm::GetPreAutoManageWindowBounds(added_window))
    ash::wm::SetPreAutoManageWindowBounds(added_window, added_bounds);
  if (MoveRectToOneSide(work_area.width(), !move_other_right, &added_bounds))
    added_window->SetBounds(added_bounds);
}

} // namespace internal

aura::Window* GetTopWindowForNewWindow(const aura::RootWindow* root_window) {
  return internal::GetReferenceWindow(root_window, NULL, NULL);
}

} // namespace ash
