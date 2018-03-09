// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_positioner.h"

#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wm/mru_window_tracker.h"
#include "ash/wm/window_positioning_utils.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/insets.h"
#include "ui/wm/core/window_animations.h"
#include "ui/wm/core/window_util.h"

namespace ash {
namespace {

// When a window gets opened in default mode and the screen is less than or
// equal to this width, the window opens with show state maximized.
const int kForceMaximizeWidthLimit = 1366;

// The time in milliseconds which should be used to visually move a window
// through an automatic "intelligent" window management option.
const int kWindowAutoMoveDurationMS = 125;

// If set to true all window repositioning actions will be ignored. Set through
// WindowPositioner::SetIgnoreActivations().
static bool disable_auto_positioning = false;

// Check if any management should be performed (with a given |window|).
bool UseAutoWindowManager(const aura::Window* window) {
  if (disable_auto_positioning)
    return false;
  const wm::WindowState* window_state = wm::GetWindowState(window);
  return !window_state->is_dragged() &&
         window_state->GetWindowPositionManaged();
}

// Check if a given |window| can be managed. This includes that its
// state is not minimized/maximized/fullscreen/the user has changed
// its size by hand already. It furthermore checks for the
// WindowIsManaged status.
bool WindowPositionCanBeManaged(const aura::Window* window) {
  if (disable_auto_positioning)
    return false;
  const wm::WindowState* window_state = wm::GetWindowState(window);
  return window_state->GetWindowPositionManaged() &&
         !window_state->IsMinimized() && !window_state->IsMaximized() &&
         !window_state->IsFullscreen() && !window_state->IsPinned() &&
         !window_state->bounds_changed_by_user();
}

// Move the given |bounds| on the available |work_area| in the direction
// indicated by |move_right|. If |move_right| is true, the rectangle gets moved
// to the right edge, otherwise to the left one.
bool MoveRectToOneSide(const gfx::Rect& work_area,
                       bool move_right,
                       gfx::Rect* bounds) {
  if (move_right) {
    if (work_area.right() > bounds->right()) {
      bounds->set_x(work_area.right() - bounds->width());
      return true;
    }
  } else {
    if (work_area.x() < bounds->x()) {
      bounds->set_x(work_area.x());
      return true;
    }
  }
  return false;
}

// Move a |window| to new |bounds|. Animate if desired by user.
// Moves the transient children of the |window| as well by the same |offset| as
// the parent |window|.
void SetBoundsAndOffsetTransientChildren(aura::Window* window,
                                         const gfx::Rect& bounds,
                                         const gfx::Rect& work_area,
                                         const gfx::Vector2d& offset) {
  aura::Window::Windows transient_children = ::wm::GetTransientChildren(window);
  for (auto* transient_child : transient_children) {
    gfx::Rect child_bounds = transient_child->bounds();
    gfx::Rect new_child_bounds = child_bounds + offset;
    if ((child_bounds.x() <= work_area.x() &&
         new_child_bounds.x() <= work_area.x()) ||
        (child_bounds.right() >= work_area.right() &&
         new_child_bounds.right() >= work_area.right())) {
      continue;
    }
    if (new_child_bounds.right() > work_area.right())
      new_child_bounds.set_x(work_area.right() - bounds.width());
    else if (new_child_bounds.x() < work_area.x())
      new_child_bounds.set_x(work_area.x());
    SetBoundsAndOffsetTransientChildren(transient_child, new_child_bounds,
                                        work_area, offset);
  }

  if (::wm::WindowAnimationsDisabled(window)) {
    window->SetBounds(bounds);
    return;
  }

  ui::ScopedLayerAnimationSettings settings(window->layer()->GetAnimator());
  settings.SetTransitionDuration(
      base::TimeDelta::FromMilliseconds(kWindowAutoMoveDurationMS));
  window->SetBounds(bounds);
}

// Move a |window| to new |bounds|. Animate if desired by user.
// Note: The function will do nothing if the bounds did not change.
void SetBoundsAnimated(aura::Window* window,
                       const gfx::Rect& bounds,
                       const gfx::Rect& work_area) {
  gfx::Rect old_bounds = window->GetTargetBounds();
  if (bounds == old_bounds)
    return;
  gfx::Vector2d offset(bounds.origin() - old_bounds.origin());
  SetBoundsAndOffsetTransientChildren(window, bounds, work_area, offset);
}

// Move |window| into the center of the screen - or restore it to the previous
// position.
void AutoPlaceSingleWindow(aura::Window* window, bool animated) {
  gfx::Rect work_area = screen_util::GetDisplayWorkAreaBoundsInParent(window);
  gfx::Rect bounds = window->bounds();
  const base::Optional<gfx::Rect> user_defined_area =
      wm::GetWindowState(window)->pre_auto_manage_window_bounds();
  if (user_defined_area) {
    bounds = *user_defined_area;
    wm::AdjustBoundsToEnsureMinimumWindowVisibility(work_area, &bounds);
  } else {
    // Center the window (only in x).
    bounds.set_x(work_area.x() + (work_area.width() - bounds.width()) / 2);
  }

  if (animated)
    SetBoundsAnimated(window, bounds, work_area);
  else
    window->SetBounds(bounds);
}

// Get the first open (non minimized) window which is on the screen defined.
aura::Window* GetReferenceWindow(const aura::Window* root_window,
                                 const aura::Window* exclude,
                                 bool* single_window) {
  if (single_window)
    *single_window = true;
  // Get the active window.
  aura::Window* active = wm::GetActiveWindow();
  if (active && active->GetRootWindow() != root_window)
    active = NULL;

  // Get a list of all windows.
  const aura::Window::Windows windows =
      Shell::Get()->mru_window_tracker()->BuildWindowListIgnoreModal();

  if (windows.empty())
    return nullptr;

  int index = 0;
  // Find the index of the current active window.
  if (active)
    index = std::find(windows.begin(), windows.end(), active) - windows.begin();

  // Scan the cycle list backwards to see which is the second topmost window
  // (and so on). Note that we might cycle a few indices twice if there is no
  // suitable window. However - since the list is fairly small this should be
  // very fast anyways.
  aura::Window* found = nullptr;
  for (int i = index + windows.size(); i >= 0; i--) {
    aura::Window* window = windows[i % windows.size()];
    while (::wm::GetTransientParent(window))
      window = ::wm::GetTransientParent(window);
    if (window != exclude &&
        window->type() == aura::client::WINDOW_TYPE_NORMAL &&
        window->GetRootWindow() == root_window && window->TargetVisibility() &&
        wm::GetWindowState(window)->GetWindowPositionManaged()) {
      if (found && found != window) {
        // no need to check !single_window because the function must have
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

}  // namespace

// static
int WindowPositioner::GetForceMaximizedWidthLimit() {
  return kForceMaximizeWidthLimit;
}

// static
void WindowPositioner::GetBoundsAndShowStateForNewWindow(
    bool is_saved_bounds,
    ui::WindowShowState show_state_in,
    gfx::Rect* bounds_in_out,
    ui::WindowShowState* show_state_out) {
  aura::Window* root_window = Shell::GetRootWindowForNewWindows();
  aura::Window* top_window = GetReferenceWindow(root_window, nullptr, nullptr);

  // If there is no valid window we take and adjust the passed coordinates
  // and show state.
  if (!top_window) {
    gfx::Rect work_area = display::Screen::GetScreen()
                              ->GetDisplayNearestWindow(root_window)
                              .work_area();
    bounds_in_out->AdjustToFit(work_area);

    // If there is no window and no saved bounds, assume first run.
    if (!is_saved_bounds && show_state_in == ui::SHOW_STATE_DEFAULT) {
      const bool maximize_first_window_on_first_run =
          Shell::Get()->shell_delegate()->IsForceMaximizeOnFirstRun();
      // We want to always open maximized on "small screens" or when policy
      // tells us to.
      const bool set_maximized =
          work_area.width() <= GetForceMaximizedWidthLimit() ||
          maximize_first_window_on_first_run;

      if (set_maximized)
        *show_state_out = ui::SHOW_STATE_MAXIMIZED;
    }
    return;
  }

  wm::WindowState* top_window_state = wm::GetWindowState(top_window);
  bool maximized = top_window_state->IsMaximized();
  // We ignore the saved show state, but look instead for the top level
  // window's show state.
  if (show_state_in == ui::SHOW_STATE_DEFAULT) {
    *show_state_out =
        maximized ? ui::SHOW_STATE_MAXIMIZED : ui::SHOW_STATE_DEFAULT;
  }

  if (maximized || top_window_state->IsFullscreen()) {
    bool has_restore_bounds = top_window_state->HasRestoreBounds();
    if (has_restore_bounds) {
      // For a maximized/fullscreen window ignore the real bounds of
      // the top level window and use its restore bounds
      // instead. Offset the bounds to prevent the windows from
      // overlapping exactly when restored.
      *bounds_in_out = top_window_state->GetRestoreBoundsInScreen() +
                       gfx::Vector2d(kFirstPopupOffset, kFirstPopupOffset);
    }
    if (is_saved_bounds || has_restore_bounds) {
      gfx::Rect work_area = display::Screen::GetScreen()
                                ->GetDisplayNearestWindow(root_window)
                                .work_area();
      bounds_in_out->AdjustToFit(work_area);
      // Use adjusted saved bounds or restore bounds, if there is one.
      return;
    }
  }

  // Use the size of the other window. The window's bound will be rearranged
  // in ash::WorkspaceLayoutManager using this location.
  *bounds_in_out = top_window->GetBoundsInScreen();
}

// static
void WindowPositioner::RearrangeVisibleWindowOnHideOrRemove(
    const aura::Window* removed_window) {
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

// static
bool WindowPositioner::DisableAutoPositioning(bool ignore) {
  bool old_state = disable_auto_positioning;
  disable_auto_positioning = ignore;
  return old_state;
}

// static
void WindowPositioner::RearrangeVisibleWindowOnShow(
    aura::Window* added_window) {
  wm::WindowState* added_window_state = wm::GetWindowState(added_window);
  if (!added_window->TargetVisibility())
    return;

  if (!UseAutoWindowManager(added_window) ||
      added_window_state->bounds_changed_by_user()) {
    if (added_window_state->minimum_visibility()) {
      // Guarantee minimum visibility within the work area.
      gfx::Rect work_area =
          screen_util::GetDisplayWorkAreaBoundsInParent(added_window);
      gfx::Rect bounds = added_window->bounds();
      gfx::Rect new_bounds = bounds;
      wm::AdjustBoundsToEnsureMinimumWindowVisibility(work_area, &new_bounds);
      if (new_bounds != bounds)
        added_window->SetBounds(new_bounds);
    }
    return;
  }
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
  gfx::Rect work_area =
      screen_util::GetDisplayWorkAreaBoundsInParent(added_window);
  bool move_other_right =
      other_bounds.CenterPoint().x() > work_area.x() + work_area.width() / 2;

  // Push the other window to the size only if there are two windows left.
  if (single_window) {
    // When going from one to two windows both windows loose their
    // "positioned by user" flags.
    added_window_state->set_bounds_changed_by_user(false);
    wm::WindowState* other_window_state =
        wm::GetWindowState(other_shown_window);
    other_window_state->set_bounds_changed_by_user(false);

    if (WindowPositionCanBeManaged(other_shown_window)) {
      // Don't override pre auto managed bounds as the current bounds
      // may not be original.
      if (!other_window_state->pre_auto_manage_window_bounds())
        other_window_state->SetPreAutoManageWindowBounds(other_bounds);

      // Push away the other window after remembering its current position.
      if (MoveRectToOneSide(work_area, move_other_right, &other_bounds))
        SetBoundsAnimated(other_shown_window, other_bounds, work_area);
    }
  }

  // Remember the current location of the window if it's new and push
  // it also to the opposite location if needed.  Since it is just
  // being shown, we do not need to animate it.
  gfx::Rect added_bounds = added_window->bounds();
  if (!added_window_state->pre_auto_manage_window_bounds())
    added_window_state->SetPreAutoManageWindowBounds(added_bounds);
  if (MoveRectToOneSide(work_area, !move_other_right, &added_bounds))
    added_window->SetBounds(added_bounds);
}

WindowPositioner::WindowPositioner() = default;

WindowPositioner::~WindowPositioner() = default;

gfx::Rect WindowPositioner::GetPopupPosition(const gfx::Size& popup_size) {
  if (!has_last_popup_position_) {
    // Start with the initial offset.
    // NOTE: This should probably move into NormalPopupPosition() but is here to
    // match historical behavior.
    last_popup_position_x_ = kFirstPopupOffset;
    last_popup_position_y_ = kFirstPopupOffset;
    has_last_popup_position_ = true;
  }
  // We handle the Multi monitor support by retrieving the active window's
  // work area.
  aura::Window* window = wm::GetActiveWindow();
  const gfx::Rect work_area =
      window && window->IsVisible()
          ? display::Screen::GetScreen()
                ->GetDisplayNearestWindow(window)
                .work_area()
          : display::Screen::GetScreen()->GetPrimaryDisplay().work_area();
  // If the popup would span the entire display, position it at top-left.
  if ((popup_size.width() + kFirstPopupOffset >= work_area.width()) ||
      (popup_size.height() + kFirstPopupOffset >= work_area.height())) {
    return AlignPopupPosition(gfx::Rect(popup_size), work_area);
  }
  const gfx::Rect result = SmartPopupPosition(popup_size, work_area);
  if (!result.IsEmpty())
    return AlignPopupPosition(result, work_area);

  return NormalPopupPosition(popup_size, work_area);
}

gfx::Rect WindowPositioner::NormalPopupPosition(const gfx::Size& popup_size,
                                                const gfx::Rect& work_area) {
  int w = popup_size.width();
  int h = popup_size.height();
  // Note: The 'last_popup_position' is checked and kept relative to the
  // screen size. The offsetting will be done in the last step when the
  // target rectangle gets returned.
  bool reset = false;
  if (last_popup_position_y_ + h > work_area.height() ||
      last_popup_position_x_ + w > work_area.width()) {
    // Popup does not fit on screen. Reset to next diagonal row.
    last_popup_position_x_ -=
        last_popup_position_y_ - kFirstPopupOffset - kNextPopupOffset;
    last_popup_position_y_ = kFirstPopupOffset;
    reset = true;
  }
  if (last_popup_position_x_ + w > work_area.width()) {
    // Start again over.
    last_popup_position_x_ = kFirstPopupOffset;
    last_popup_position_y_ = kFirstPopupOffset;
    reset = true;
  }
  int x = last_popup_position_x_;
  int y = last_popup_position_y_;
  if (!reset) {
    last_popup_position_x_ += kNextPopupOffset;
    last_popup_position_y_ += kNextPopupOffset;
  }
  return gfx::Rect(x + work_area.x(), y + work_area.y(), w, h);
}

// static
gfx::Rect WindowPositioner::SmartPopupPosition(const gfx::Size& popup_size,
                                               const gfx::Rect& work_area) {
  const aura::Window::Windows windows =
      Shell::Get()->mru_window_tracker()->BuildWindowListIgnoreModal();

  std::vector<const gfx::Rect*> regions;
  // Process the window list and check if we can bail immediately.
  for (size_t i = 0; i < windows.size(); i++) {
    // We only include opaque and visible windows.
    if (windows[i] && windows[i]->IsVisible() && windows[i]->layer() &&
        (windows[i]->layer()->fills_bounds_opaquely() ||
         windows[i]->layer()->GetTargetOpacity() == 1.0)) {
      wm::WindowState* window_state = wm::GetWindowState(windows[i]);
      // When any window is maximized we cannot find any free space.
      if (window_state->IsMaximizedOrFullscreenOrPinned())
        return gfx::Rect(0, 0, 0, 0);
      if (window_state->IsNormalOrSnapped())
        regions.push_back(&windows[i]->bounds());
    }
  }

  if (regions.empty())
    return gfx::Rect(0, 0, 0, 0);

  int w = popup_size.width();
  int h = popup_size.height();
  int x_end = work_area.width() / 2;
  int x, x_increment;
  // We parse for a proper location on the screen. We do this in two runs:
  // The first run will start from the left, parsing down, skipping any
  // overlapping windows it will encounter until the popup's height can not
  // be served anymore. Then the next grid position to the right will be
  // taken, and the same cycle starts again. This will be repeated until we
  // hit the middle of the screen (or we find a suitable location).
  // In the second run we parse beginning from the right corner downwards and
  // then to the left.
  // When no location was found, an empty rectangle will be returned.
  for (int run = 0; run < 2; run++) {
    if (run == 0) {  // First run: Start left, parse right till mid screen.
      x = 0;
      x_increment = kNextPopupOffset;
    } else {  // Second run: Start right, parse left till mid screen.
      x = work_area.width() - w;
      x_increment = -kNextPopupOffset;
    }
    // Note: The passing (x,y,w,h) window is always relative to the work area's
    // origin.
    for (; x_increment > 0 ? (x < x_end) : (x > x_end); x += x_increment) {
      int y = 0;
      while (y + h <= work_area.height()) {
        size_t i;
        for (i = 0; i < regions.size(); i++) {
          if (regions[i]->Intersects(
                  gfx::Rect(x + work_area.x(), y + work_area.y(), w, h))) {
            y = regions[i]->bottom() - work_area.y();
            break;
          }
        }
        if (i >= regions.size())
          return gfx::Rect(x + work_area.x(), y + work_area.y(), w, h);
      }
    }
  }
  return gfx::Rect(0, 0, 0, 0);
}

// static
gfx::Rect WindowPositioner::AlignPopupPosition(const gfx::Rect& pos,
                                               const gfx::Rect& work_area) {
  int x = pos.x() - (pos.x() - work_area.x()) % kPopupGridSize;
  int y = pos.y() - (pos.y() - work_area.y()) % kPopupGridSize;
  int w = pos.width();
  int h = pos.height();

  // If the alignment was pushing the window out of the screen, we ignore the
  // alignment for that call.
  if (abs(pos.right() - work_area.right()) < kPopupGridSize)
    x = work_area.right() - w;
  if (abs(pos.bottom() - work_area.bottom()) < kPopupGridSize)
    y = work_area.bottom() - h;
  return gfx::Rect(x, y, w, h);
}

}  // namespace ash
