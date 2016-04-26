// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/maximize_mode/maximize_mode_window_state.h"

#include <utility>

#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/aura/wm_window_aura.h"
#include "ash/wm/common/window_animation_types.h"
#include "ash/wm/common/window_state_util.h"
#include "ash/wm/common/wm_event.h"
#include "ash/wm/common/wm_screen_util.h"
#include "ash/wm/maximize_mode/maximize_mode_window_manager.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_properties.h"
#include "ash/wm/window_state_aura.h"
#include "ash/wm/window_state_delegate.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/workspace_window_resizer.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/compositor/layer.h"
#include "ui/gfx/display.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/view_constants_aura.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// Returns the biggest possible size for a window which is about to be
// maximized.
gfx::Size GetMaximumSizeOfWindow(wm::WindowState* window_state) {
  DCHECK(window_state->CanMaximize() || window_state->CanResize());

  gfx::Size workspace_size =
      wm::GetMaximizedWindowBoundsInParent(window_state->window()).size();

  gfx::Size size = window_state->window()->GetMaximumSize();
  if (size.IsEmpty())
    return workspace_size;

  size.SetToMin(workspace_size);
  return size;
}

// Returns the centered bounds of the given bounds in the work area.
gfx::Rect GetCenteredBounds(const gfx::Rect& bounds_in_parent,
                            wm::WindowState* state_object) {
  gfx::Rect work_area_in_parent =
      wm::GetDisplayWorkAreaBoundsInParent(state_object->window());
  work_area_in_parent.ClampToCenteredSize(bounds_in_parent.size());
  return work_area_in_parent;
}

// Returns the maximized/full screen and/or centered bounds of a window.
gfx::Rect GetBoundsInMaximizedMode(wm::WindowState* state_object) {
  if (state_object->IsFullscreen())
    return wm::GetDisplayBoundsInParent(state_object->window());

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
      bounds_in_parent = state_object->window()->GetBounds();
  }
  return GetCenteredBounds(bounds_in_parent, state_object);
}

}  // namespace

// static
void MaximizeModeWindowState::UpdateWindowPosition(
    wm::WindowState* window_state) {
  gfx::Rect bounds_in_parent = GetBoundsInMaximizedMode(window_state);
  if (bounds_in_parent == window_state->window()->GetBounds())
    return;
  window_state->SetBoundsDirect(bounds_in_parent);
}

MaximizeModeWindowState::MaximizeModeWindowState(
    aura::Window* window, MaximizeModeWindowManager* creator)
    : window_(window),
      creator_(creator),
      current_state_type_(wm::GetWindowState(window)->GetStateType()),
      defer_bounds_updates_(false) {
  old_state_.reset(wm::GetWindowState(window)
                       ->SetStateObject(std::unique_ptr<State>(this))
                       .release());
}

MaximizeModeWindowState::~MaximizeModeWindowState() {
  creator_->WindowStateDestroyed(window_);
}

void MaximizeModeWindowState::LeaveMaximizeMode(wm::WindowState* window_state) {
  // Note: When we return we will destroy ourselves with the |our_reference|.
  std::unique_ptr<wm::WindowState::State> our_reference =
      window_state->SetStateObject(std::move(old_state_));
}

void MaximizeModeWindowState::SetDeferBoundsUpdates(bool defer_bounds_updates) {
  if (defer_bounds_updates_ == defer_bounds_updates)
    return;

  defer_bounds_updates_ = defer_bounds_updates;
  if (!defer_bounds_updates_)
    UpdateBounds(wm::GetWindowState(window_), true);
}

void MaximizeModeWindowState::OnWMEvent(wm::WindowState* window_state,
                                        const wm::WMEvent* event) {
  switch (event->type()) {
    case wm::WM_EVENT_TOGGLE_FULLSCREEN:
      ToggleFullScreen(window_state, window_state->delegate());
      break;
    case wm::WM_EVENT_FULLSCREEN:
      UpdateWindow(window_state, wm::WINDOW_STATE_TYPE_FULLSCREEN, true);
      break;
    case wm::WM_EVENT_TOGGLE_MAXIMIZE_CAPTION:
    case wm::WM_EVENT_TOGGLE_VERTICAL_MAXIMIZE:
    case wm::WM_EVENT_TOGGLE_HORIZONTAL_MAXIMIZE:
    case wm::WM_EVENT_TOGGLE_MAXIMIZE:
    case wm::WM_EVENT_CYCLE_SNAP_DOCK_LEFT:
    case wm::WM_EVENT_CYCLE_SNAP_DOCK_RIGHT:
    case wm::WM_EVENT_CENTER:
    case wm::WM_EVENT_SNAP_LEFT:
    case wm::WM_EVENT_SNAP_RIGHT:
    case wm::WM_EVENT_NORMAL:
    case wm::WM_EVENT_MAXIMIZE:
    case wm::WM_EVENT_DOCK:
      UpdateWindow(window_state,
                   GetMaximizedOrCenteredWindowType(window_state),
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
                 current_state_type_ != wm::WINDOW_STATE_TYPE_MAXIMIZED &&
                 current_state_type_ != wm::WINDOW_STATE_TYPE_FULLSCREEN) {
        // In all other cases (except for minimized windows) we respect the
        // requested bounds and center it to a fully visible area on the screen.
        gfx::Rect bounds_in_parent =
            (static_cast<const wm::SetBoundsEvent*>(event))->requested_bounds();
        bounds_in_parent = GetCenteredBounds(bounds_in_parent, window_state);
        if (bounds_in_parent != window_state->window()->GetBounds()) {
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

wm::WindowStateType MaximizeModeWindowState::GetType() const {
  return current_state_type_;
}

void MaximizeModeWindowState::AttachState(
    wm::WindowState* window_state,
    wm::WindowState::State* previous_state) {
  current_state_type_ = previous_state->GetType();

  aura::Window* window =
      ash::wm::WmWindowAura::GetAuraWindow(window_state->window());
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
  if (widget) {
    gfx::Rect bounds = widget->GetRestoredBounds();
    if (!bounds.IsEmpty()) {
      // We do not want to do a session restore to our window states. Therefore
      // we tell the window to use the current default states instead.
      window->SetProperty(ash::kRestoreShowStateOverrideKey,
                          window_state->GetShowState());
      window->SetProperty(ash::kRestoreBoundsOverrideKey,
                          new gfx::Rect(widget->GetRestoredBounds()));
    }
  }

  // Initialize the state to a good preset.
  if (current_state_type_ != wm::WINDOW_STATE_TYPE_MAXIMIZED &&
      current_state_type_ != wm::WINDOW_STATE_TYPE_MINIMIZED &&
      current_state_type_ != wm::WINDOW_STATE_TYPE_FULLSCREEN) {
    UpdateWindow(window_state,
                 GetMaximizedOrCenteredWindowType(window_state),
                 true);
  }

  window_state->set_can_be_dragged(false);
}

void MaximizeModeWindowState::DetachState(wm::WindowState* window_state) {
  // From now on, we can use the default session restore mechanism again.
  ash::wm::WmWindowAura::GetAuraWindow(window_state->window())
      ->ClearProperty(ash::kRestoreBoundsOverrideKey);
  window_state->set_can_be_dragged(true);
}

void MaximizeModeWindowState::UpdateWindow(wm::WindowState* window_state,
                                           wm::WindowStateType target_state,
                                           bool animated) {
  DCHECK(target_state == wm::WINDOW_STATE_TYPE_MINIMIZED ||
         target_state == wm::WINDOW_STATE_TYPE_MAXIMIZED ||
         (target_state == wm::WINDOW_STATE_TYPE_NORMAL &&
              !window_state->CanMaximize()) ||
         target_state == wm::WINDOW_STATE_TYPE_FULLSCREEN);

  if (target_state == wm::WINDOW_STATE_TYPE_MINIMIZED) {
    if (current_state_type_ == wm::WINDOW_STATE_TYPE_MINIMIZED)
      return;

    current_state_type_ = target_state;
    window_state->window()->SetVisibilityAnimationType(
        wm::WINDOW_VISIBILITY_ANIMATION_TYPE_MINIMIZE);
    window_state->window()->Hide();
    if (window_state->IsActive())
      window_state->Deactivate();
    return;
  }

  if (current_state_type_ == target_state) {
    // If the state type did not change, update it accordingly.
    UpdateBounds(window_state, animated);
    return;
  }

  const wm::WindowStateType old_state_type = current_state_type_;
  current_state_type_ = target_state;
  window_state->UpdateWindowShowStateFromStateType();
  window_state->NotifyPreStateTypeChange(old_state_type);
  UpdateBounds(window_state, animated);
  window_state->NotifyPostStateTypeChange(old_state_type);

  if ((window_state->window()->GetTargetVisibility() ||
       old_state_type == wm::WINDOW_STATE_TYPE_MINIMIZED) &&
      !window_state->window()->GetLayer()->visible()) {
    // The layer may be hidden if the window was previously minimized. Make
    // sure it's visible.
    window_state->window()->Show();
  }
}

wm::WindowStateType MaximizeModeWindowState::GetMaximizedOrCenteredWindowType(
      wm::WindowState* window_state) {
  return window_state->CanMaximize() ? wm::WINDOW_STATE_TYPE_MAXIMIZED :
                                       wm::WINDOW_STATE_TYPE_NORMAL;
}

void MaximizeModeWindowState::UpdateBounds(wm::WindowState* window_state,
                                           bool animated) {
  if (defer_bounds_updates_)
    return;
  gfx::Rect bounds_in_parent = GetBoundsInMaximizedMode(window_state);
  // If we have a target bounds rectangle, we center it and set it
  // accordingly.
  if (!bounds_in_parent.IsEmpty() &&
      bounds_in_parent != window_state->window()->GetBounds()) {
    if (current_state_type_ == wm::WINDOW_STATE_TYPE_MINIMIZED ||
        !window_state->window()->IsVisible() ||
        !animated) {
      window_state->SetBoundsDirect(bounds_in_parent);
    } else {
      // If we animate (to) maximized mode, we want to use the cross fade to
      // avoid flashing.
      if (window_state->IsMaximized())
        window_state->SetBoundsDirectCrossFade(bounds_in_parent);
      else
        window_state->SetBoundsDirectAnimated(bounds_in_parent);
    }
  }
}

}  // namespace ash
