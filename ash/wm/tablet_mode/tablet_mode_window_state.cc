// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/tablet_mode/tablet_mode_window_state.h"

#include <utility>

#include "ash/public/cpp/shell_window_ids.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/wm/screen_pinning_controller.h"
#include "ash/wm/splitview/split_view_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_window_manager.h"
#include "ash/wm/window_animation_types.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state_util.h"
#include "ash/wm/wm_event.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {
namespace {

// Sets the restore bounds and show state overrides. These values take
// precedence over the restore bounds and restore show state (if set).
// If |bounds_override| is empty the values are cleared.
void SetWindowRestoreOverrides(aura::Window* window,
                               const gfx::Rect& bounds_override,
                               ui::WindowShowState window_state_override) {
  if (bounds_override.IsEmpty()) {
    window->ClearProperty(kRestoreShowStateOverrideKey);
    window->ClearProperty(kRestoreBoundsOverrideKey);
    return;
  }
  window->SetProperty(kRestoreShowStateOverrideKey, window_state_override);
  window->SetProperty(kRestoreBoundsOverrideKey,
                      new gfx::Rect(bounds_override));
}

// Returns the biggest possible size for a window which is about to be
// maximized.
gfx::Size GetMaximumSizeOfWindow(wm::WindowState* window_state) {
  DCHECK(window_state->CanMaximize() || window_state->CanResize());

  gfx::Size workspace_size =
      ScreenUtil::GetMaximizedWindowBoundsInParent(window_state->window())
          .size();

  gfx::Size size = window_state->window()->delegate()
                       ? window_state->window()->delegate()->GetMaximumSize()
                       : gfx::Size();
  if (size.IsEmpty())
    return workspace_size;

  size.SetToMin(workspace_size);
  return size;
}

// Returns the centered bounds of the given bounds in the work area.
gfx::Rect GetCenteredBounds(const gfx::Rect& bounds_in_parent,
                            wm::WindowState* state_object) {
  gfx::Rect work_area_in_parent =
      ScreenUtil::GetDisplayWorkAreaBoundsInParent(state_object->window());
  work_area_in_parent.ClampToCenteredSize(bounds_in_parent.size());
  return work_area_in_parent;
}

// Returns true if the window can snap in tablet mode.
bool CanSnap(wm::WindowState* window_state) {
  // If split view mode is not allowed in tablet mode, do not allow snapping
  // windows.
  if (!SplitViewController::ShouldAllowSplitView())
    return false;
  return window_state->CanSnap();
}

// Returns the maximized/full screen and/or centered bounds of a window.
gfx::Rect GetBoundsInMaximizedMode(wm::WindowState* state_object) {
  if (state_object->IsFullscreen() || state_object->IsPinned())
    return ScreenUtil::GetDisplayBoundsInParent(state_object->window());

  if (state_object->GetStateType() == wm::WINDOW_STATE_TYPE_LEFT_SNAPPED) {
    DCHECK(CanSnap(state_object));
    return Shell::Get()
        ->split_view_controller()
        ->GetSnappedWindowBoundsInParent(state_object->window(),
                                         SplitViewController::LEFT_SNAPPED);
  }

  if (state_object->GetStateType() == wm::WINDOW_STATE_TYPE_RIGHT_SNAPPED) {
    DCHECK(CanSnap(state_object));
    return Shell::Get()
        ->split_view_controller()
        ->GetSnappedWindowBoundsInParent(state_object->window(),
                                         SplitViewController::RIGHT_SNAPPED);
  }

  gfx::Rect bounds_in_parent;
  // Make the window as big as possible.
  if (state_object->CanMaximize() || state_object->CanResize()) {
    bounds_in_parent.set_size(GetMaximumSizeOfWindow(state_object));
  } else {
    // We prefer the user given window dimensions over the current windows
    // dimensions since they are likely to be the result from some other state
    // object logic.
    if (state_object->HasRestoreBounds())
      bounds_in_parent = state_object->GetRestoreBoundsInParent();
    else
      bounds_in_parent = state_object->window()->bounds();
  }
  return GetCenteredBounds(bounds_in_parent, state_object);
}

gfx::Rect GetRestoreBounds(wm::WindowState* window_state) {
  if (window_state->IsMinimized() || window_state->IsMaximized() ||
      window_state->IsFullscreen()) {
    gfx::Rect restore_bounds = window_state->GetRestoreBoundsInScreen();
    if (!restore_bounds.IsEmpty())
      return restore_bounds;
  }
  return window_state->window()->GetBoundsInScreen();
}

}  // namespace

// static
void TabletModeWindowState::UpdateWindowPosition(
    wm::WindowState* window_state) {
  gfx::Rect bounds_in_parent = GetBoundsInMaximizedMode(window_state);
  if (bounds_in_parent == window_state->window()->bounds())
    return;
  window_state->SetBoundsDirect(bounds_in_parent);
}

TabletModeWindowState::TabletModeWindowState(aura::Window* window,
                                             TabletModeWindowManager* creator)
    : window_(window),
      creator_(creator),
      current_state_type_(wm::GetWindowState(window)->GetStateType()),
      defer_bounds_updates_(false) {
  old_state_.reset(wm::GetWindowState(window)
                       ->SetStateObject(std::unique_ptr<State>(this))
                       .release());
}

TabletModeWindowState::~TabletModeWindowState() {
  creator_->WindowStateDestroyed(window_);
}

void TabletModeWindowState::LeaveTabletMode(wm::WindowState* window_state) {
  // Note: When we return we will destroy ourselves with the |our_reference|.
  std::unique_ptr<wm::WindowState::State> our_reference =
      window_state->SetStateObject(std::move(old_state_));
}

void TabletModeWindowState::SetDeferBoundsUpdates(bool defer_bounds_updates) {
  if (defer_bounds_updates_ == defer_bounds_updates)
    return;

  defer_bounds_updates_ = defer_bounds_updates;
  if (!defer_bounds_updates_)
    UpdateBounds(wm::GetWindowState(window_), true);
}

void TabletModeWindowState::OnWMEvent(wm::WindowState* window_state,
                                      const wm::WMEvent* event) {
  // Ignore events that are sent during the exit transition.
  if (ignore_wm_events_) {
    return;
  }

  switch (event->type()) {
    case wm::WM_EVENT_TOGGLE_FULLSCREEN:
      ToggleFullScreen(window_state, window_state->delegate());
      break;
    case wm::WM_EVENT_FULLSCREEN:
      UpdateWindow(window_state, wm::WINDOW_STATE_TYPE_FULLSCREEN, true);
      break;
    case wm::WM_EVENT_PIN:
      if (!Shell::Get()->screen_pinning_controller()->IsPinned())
        UpdateWindow(window_state, wm::WINDOW_STATE_TYPE_PINNED, true);
      break;
    case wm::WM_EVENT_TRUSTED_PIN:
      if (!Shell::Get()->screen_pinning_controller()->IsPinned())
        UpdateWindow(window_state, wm::WINDOW_STATE_TYPE_TRUSTED_PINNED, true);
      break;
    case wm::WM_EVENT_TOGGLE_MAXIMIZE_CAPTION:
    case wm::WM_EVENT_TOGGLE_VERTICAL_MAXIMIZE:
    case wm::WM_EVENT_TOGGLE_HORIZONTAL_MAXIMIZE:
    case wm::WM_EVENT_TOGGLE_MAXIMIZE:
    case wm::WM_EVENT_CYCLE_SNAP_LEFT:
    case wm::WM_EVENT_CYCLE_SNAP_RIGHT:
    case wm::WM_EVENT_CENTER:
    case wm::WM_EVENT_NORMAL:
    case wm::WM_EVENT_MAXIMIZE:
      UpdateWindow(window_state, GetMaximizedOrCenteredWindowType(window_state),
                   true);
      return;
    case wm::WM_EVENT_SNAP_LEFT:
      UpdateWindow(window_state,
                   GetSnappedWindowStateType(
                       window_state, wm::WINDOW_STATE_TYPE_LEFT_SNAPPED),
                   true);
      return;
    case wm::WM_EVENT_SNAP_RIGHT:
      UpdateWindow(window_state,
                   GetSnappedWindowStateType(
                       window_state, wm::WINDOW_STATE_TYPE_RIGHT_SNAPPED),
                   true);
      return;
    case wm::WM_EVENT_MINIMIZE:
      UpdateWindow(window_state, wm::WINDOW_STATE_TYPE_MINIMIZED, true);
      return;
    case wm::WM_EVENT_SHOW_INACTIVE:
      return;
    case wm::WM_EVENT_SET_BOUNDS:
      if (current_state_type_ == wm::WINDOW_STATE_TYPE_MAXIMIZED) {
        // Having a maximized window, it could have been created with an empty
        // size and the caller should get his size upon leaving the maximized
        // mode. As such we set the restore bounds to the requested bounds.
        gfx::Rect bounds_in_parent =
            (static_cast<const wm::SetBoundsEvent*>(event))->requested_bounds();
        if (!bounds_in_parent.IsEmpty())
          window_state->SetRestoreBoundsInParent(bounds_in_parent);
      } else if (current_state_type_ != wm::WINDOW_STATE_TYPE_MINIMIZED &&
                 current_state_type_ != wm::WINDOW_STATE_TYPE_FULLSCREEN &&
                 current_state_type_ != wm::WINDOW_STATE_TYPE_PINNED &&
                 current_state_type_ != wm::WINDOW_STATE_TYPE_TRUSTED_PINNED &&
                 current_state_type_ != wm::WINDOW_STATE_TYPE_LEFT_SNAPPED &&
                 current_state_type_ != wm::WINDOW_STATE_TYPE_RIGHT_SNAPPED) {
        // In all other cases (except for minimized windows) we respect the
        // requested bounds and center it to a fully visible area on the screen.
        gfx::Rect bounds_in_parent =
            (static_cast<const wm::SetBoundsEvent*>(event))->requested_bounds();
        bounds_in_parent = GetCenteredBounds(bounds_in_parent, window_state);
        if (bounds_in_parent != window_state->window()->bounds()) {
          if (window_state->window()->IsVisible())
            window_state->SetBoundsDirectAnimated(bounds_in_parent);
          else
            window_state->SetBoundsDirect(bounds_in_parent);
        }
      }
      break;
    case wm::WM_EVENT_ADDED_TO_WORKSPACE:
      if (current_state_type_ != wm::WINDOW_STATE_TYPE_MAXIMIZED &&
          current_state_type_ != wm::WINDOW_STATE_TYPE_FULLSCREEN &&
          current_state_type_ != wm::WINDOW_STATE_TYPE_MINIMIZED) {
        wm::WindowStateType new_state =
            GetMaximizedOrCenteredWindowType(window_state);
        UpdateWindow(window_state, new_state, true);
      }
      break;
    case wm::WM_EVENT_WORKAREA_BOUNDS_CHANGED:
      if (current_state_type_ != wm::WINDOW_STATE_TYPE_MINIMIZED)
        UpdateBounds(window_state, true);
      break;
    case wm::WM_EVENT_DISPLAY_BOUNDS_CHANGED:
      // Don't animate on a screen rotation - just snap to new size.
      if (current_state_type_ != wm::WINDOW_STATE_TYPE_MINIMIZED)
        UpdateBounds(window_state, false);
      break;
  }
}

wm::WindowStateType TabletModeWindowState::GetType() const {
  return current_state_type_;
}

void TabletModeWindowState::AttachState(
    wm::WindowState* window_state,
    wm::WindowState::State* previous_state) {
  current_state_type_ = previous_state->GetType();

  gfx::Rect restore_bounds = GetRestoreBounds(window_state);
  if (!restore_bounds.IsEmpty()) {
    // We do not want to do a session restore to our window states. Therefore
    // we tell the window to use the current default states instead.
    SetWindowRestoreOverrides(window_state->window(), restore_bounds,
                              window_state->GetShowState());
  }

  // Initialize the state to a good preset.
  if (current_state_type_ != wm::WINDOW_STATE_TYPE_MAXIMIZED &&
      current_state_type_ != wm::WINDOW_STATE_TYPE_MINIMIZED &&
      current_state_type_ != wm::WINDOW_STATE_TYPE_FULLSCREEN &&
      current_state_type_ != wm::WINDOW_STATE_TYPE_PINNED &&
      current_state_type_ != wm::WINDOW_STATE_TYPE_TRUSTED_PINNED) {
    UpdateWindow(window_state, GetMaximizedOrCenteredWindowType(window_state),
                 true);
  }

  window_state->set_can_be_dragged(false);
}

void TabletModeWindowState::DetachState(wm::WindowState* window_state) {
  // From now on, we can use the default session restore mechanism again.
  SetWindowRestoreOverrides(window_state->window(), gfx::Rect(),
                            ui::SHOW_STATE_NORMAL);
  window_state->set_can_be_dragged(true);
}

void TabletModeWindowState::UpdateWindow(wm::WindowState* window_state,
                                         wm::WindowStateType target_state,
                                         bool animated) {
  DCHECK(target_state == wm::WINDOW_STATE_TYPE_MINIMIZED ||
         target_state == wm::WINDOW_STATE_TYPE_MAXIMIZED ||
         target_state == wm::WINDOW_STATE_TYPE_PINNED ||
         target_state == wm::WINDOW_STATE_TYPE_TRUSTED_PINNED ||
         (target_state == wm::WINDOW_STATE_TYPE_NORMAL &&
          !window_state->CanMaximize()) ||
         target_state == wm::WINDOW_STATE_TYPE_FULLSCREEN ||
         target_state == wm::WINDOW_STATE_TYPE_LEFT_SNAPPED ||
         target_state == wm::WINDOW_STATE_TYPE_RIGHT_SNAPPED);

  if (current_state_type_ == target_state) {
    if (target_state == wm::WINDOW_STATE_TYPE_MINIMIZED)
      return;
    // If the state type did not change, update it accordingly.
    UpdateBounds(window_state, animated);
    return;
  }

  const wm::WindowStateType old_state_type = current_state_type_;
  current_state_type_ = target_state;
  window_state->UpdateWindowPropertiesFromStateType();
  window_state->NotifyPreStateTypeChange(old_state_type);

  if (target_state == wm::WINDOW_STATE_TYPE_MINIMIZED) {
    ::wm::SetWindowVisibilityAnimationType(
        window_state->window(), wm::WINDOW_VISIBILITY_ANIMATION_TYPE_MINIMIZE);
    window_state->window()->Hide();
    if (window_state->IsActive())
      window_state->Deactivate();
  } else if (target_state == wm::WINDOW_STATE_TYPE_LEFT_SNAPPED) {
    window_state->SetBoundsDirectAnimated(
        Shell::Get()->split_view_controller()->GetSnappedWindowBoundsInParent(
            window_state->window(), SplitViewController::LEFT_SNAPPED));
  } else if (target_state == wm::WINDOW_STATE_TYPE_RIGHT_SNAPPED) {
    window_state->SetBoundsDirectAnimated(
        Shell::Get()->split_view_controller()->GetSnappedWindowBoundsInParent(
            window_state->window(), SplitViewController::RIGHT_SNAPPED));
  } else {
    UpdateBounds(window_state, animated);
  }

  window_state->NotifyPostStateTypeChange(old_state_type);

  if (old_state_type == wm::WINDOW_STATE_TYPE_PINNED ||
      target_state == wm::WINDOW_STATE_TYPE_PINNED ||
      old_state_type == wm::WINDOW_STATE_TYPE_TRUSTED_PINNED ||
      target_state == wm::WINDOW_STATE_TYPE_TRUSTED_PINNED) {
    Shell::Get()->screen_pinning_controller()->SetPinnedWindow(
        window_state->window());
  }

  if ((window_state->window()->layer()->GetTargetVisibility() ||
       old_state_type == wm::WINDOW_STATE_TYPE_MINIMIZED) &&
      !window_state->window()->layer()->visible()) {
    // The layer may be hidden if the window was previously minimized. Make
    // sure it's visible.
    window_state->window()->Show();
  }
}

wm::WindowStateType TabletModeWindowState::GetMaximizedOrCenteredWindowType(
    wm::WindowState* window_state) {
  return window_state->CanMaximize() ? wm::WINDOW_STATE_TYPE_MAXIMIZED
                                     : wm::WINDOW_STATE_TYPE_NORMAL;
}

wm::WindowStateType TabletModeWindowState::GetSnappedWindowStateType(
    wm::WindowState* window_state,
    wm::WindowStateType target_state) {
  DCHECK(target_state == wm::WINDOW_STATE_TYPE_LEFT_SNAPPED ||
         target_state == wm::WINDOW_STATE_TYPE_RIGHT_SNAPPED);
  return CanSnap(window_state) ? target_state
                               : GetMaximizedOrCenteredWindowType(window_state);
}

void TabletModeWindowState::UpdateBounds(wm::WindowState* window_state,
                                         bool animated) {
  if (defer_bounds_updates_)
    return;
  gfx::Rect bounds_in_parent = GetBoundsInMaximizedMode(window_state);
  // If we have a target bounds rectangle, we center it and set it
  // accordingly.
  if (!bounds_in_parent.IsEmpty() &&
      bounds_in_parent != window_state->window()->bounds()) {
    if (current_state_type_ == wm::WINDOW_STATE_TYPE_MINIMIZED ||
        !window_state->window()->IsVisible() || !animated) {
      window_state->SetBoundsDirect(bounds_in_parent);
    } else {
      // If we animate (to) tablet mode, we want to use the cross fade to
      // avoid flashing.
      if (window_state->IsMaximized())
        window_state->SetBoundsDirectCrossFade(bounds_in_parent);
      else
        window_state->SetBoundsDirectAnimated(bounds_in_parent);
    }
  }
}

}  // namespace ash
