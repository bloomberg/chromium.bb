// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/maximize_mode/maximize_mode_window_state.h"

#include "ash/display/display_controller.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/coordinate_conversion.h"
#include "ash/wm/maximize_mode/maximize_mode_window_manager.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_delegate.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/workspace/workspace_window_resizer.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/gfx/display.h"
#include "ui/gfx/rect.h"

namespace ash {
namespace {

// Returns the biggest possible size for a window which is about to be
// maximized.
gfx::Size GetMaximumSizeOfWindow(wm::WindowState* window_state) {
  DCHECK(window_state->CanMaximize());

  gfx::Size workspace_size = ScreenUtil::GetMaximizedWindowBoundsInParent(
      window_state->window()).size();

  aura::WindowDelegate* delegate = window_state->window()->delegate();
  if (!delegate)
    return workspace_size;

  gfx::Size size = delegate->GetMaximumSize();
  if (size.IsEmpty())
    return workspace_size;

  size.SetToMin(workspace_size);
  return size;
}

// Returns the maximized and centered bounds of a window.
gfx::Rect GetMaximizedAndCenteredBounds(wm::WindowState* state_object) {
  gfx::Rect bounds_in_parent;
  if (state_object->CanMaximize()) {
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
  gfx::Rect work_area_in_parent =
      ScreenUtil::GetDisplayWorkAreaBoundsInParent(state_object->window());

  wm::AdjustBoundsToEnsureMinimumWindowVisibility(work_area_in_parent,
                                                  &bounds_in_parent);

  // Center the window over the work area.
  int x = (work_area_in_parent.width() - bounds_in_parent.width()) / 2 +
          work_area_in_parent.x();
  int y = (work_area_in_parent.height() - bounds_in_parent.height()) / 2 +
          work_area_in_parent.y();

  bounds_in_parent.set_origin(gfx::Point(x, y));

  return bounds_in_parent;
}

}  // namespace

// static
void MaximizeModeWindowState::UpdateWindowPosition(
    wm::WindowState* window_state, bool animated) {
  gfx::Rect bounds_in_parent = GetMaximizedAndCenteredBounds(window_state);

  if (bounds_in_parent == window_state->window()->bounds())
    return;

  if (animated)
    window_state->SetBoundsDirect(bounds_in_parent);
  else
    window_state->SetBoundsDirectAnimated(bounds_in_parent);
}

MaximizeModeWindowState::MaximizeModeWindowState(
    aura::Window* window, MaximizeModeWindowManager* creator)
    : window_(window),
      creator_(creator),
      current_state_type_(wm::GetWindowState(window)->GetStateType()) {
  old_state_.reset(
      wm::GetWindowState(window)->SetStateObject(
          scoped_ptr<State>(this).Pass()).release());
}

MaximizeModeWindowState::~MaximizeModeWindowState() {
  creator_->WindowStateDestroyed(window_);
}

void MaximizeModeWindowState::LeaveMaximizeMode(wm::WindowState* window_state) {
  // Note: When we return we will destroy ourselves with the |our_reference|.
  scoped_ptr<wm::WindowState::State> our_reference =
      window_state->SetStateObject(old_state_.Pass());
}

void MaximizeModeWindowState::OnWMEvent(wm::WindowState* window_state,
                                        const wm::WMEvent* event) {
  switch (event->type()) {
    case wm::WM_EVENT_TOGGLE_MAXIMIZE_CAPTION:
    case wm::WM_EVENT_TOGGLE_VERTICAL_MAXIMIZE:
    case wm::WM_EVENT_TOGGLE_HORIZONTAL_MAXIMIZE:
    case wm::WM_EVENT_TOGGLE_FULLSCREEN:
    case wm::WM_EVENT_TOGGLE_MAXIMIZE:
    case wm::WM_EVENT_CENTER:
    case wm::WM_EVENT_FULLSCREEN:
    case wm::WM_EVENT_SNAP_LEFT:
    case wm::WM_EVENT_SNAP_RIGHT:
    case wm::WM_EVENT_NORMAL:
    case wm::WM_EVENT_MAXIMIZE:
      MaximizeOrCenterWindow(window_state, true);
      return;
    case wm::WM_EVENT_MINIMIZE:
      if (current_state_type_ != wm::WINDOW_STATE_TYPE_MINIMIZED) {
        current_state_type_ = wm::WINDOW_STATE_TYPE_MINIMIZED;
        Minimize(window_state);
      }
      return;
    case wm::WM_EVENT_SHOW_INACTIVE:
      return;
    case wm::WM_EVENT_SET_BOUNDS:
      if (current_state_type_ != wm::WINDOW_STATE_TYPE_MINIMIZED &&
          current_state_type_ != wm::WINDOW_STATE_TYPE_MAXIMIZED) {
        gfx::Rect bounds_in_parent =
            (static_cast<const wm::SetBoundsEvent*>(event))->requested_bounds();
        bounds_in_parent.ClampToCenteredSize(
             ScreenUtil::GetDisplayWorkAreaBoundsInParent(
                 window_state->window()).size());
        if (bounds_in_parent != window_state->window()->bounds())
          window_state->SetBoundsDirectAnimated(bounds_in_parent);
      }
      break;
    case wm::WM_EVENT_ADDED_TO_WORKSPACE:
      MaximizeOrCenterWindow(window_state, true);
      break;
    case wm::WM_EVENT_WORKAREA_BOUNDS_CHANGED:
    case wm::WM_EVENT_DISPLAY_BOUNDS_CHANGED:
      if (current_state_type_ != wm::WINDOW_STATE_TYPE_MINIMIZED)
        MaximizeOrCenterWindow(window_state, false);
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

  // Initialize the state to a good preset.
  if (current_state_type_ != wm::WINDOW_STATE_TYPE_MAXIMIZED &&
      current_state_type_ != wm::WINDOW_STATE_TYPE_MINIMIZED) {
    MaximizeOrCenterWindow(window_state, true);
  }

  window_state->set_can_be_dragged(false);
}

void MaximizeModeWindowState::DetachState(wm::WindowState* window_state) {
  window_state->set_can_be_dragged(true);
}

void MaximizeModeWindowState::MaximizeOrCenterWindow(
    wm::WindowState* window_state,
    bool animated) {
  const wm::WindowStateType target_state =
      window_state->CanMaximize() ? wm::WINDOW_STATE_TYPE_MAXIMIZED :
                                    wm::WINDOW_STATE_TYPE_NORMAL;
  const wm::WindowStateType old_state_type = current_state_type_;
  gfx::Rect bounds_in_parent = GetMaximizedAndCenteredBounds(window_state);

  if (current_state_type_ != target_state) {
    current_state_type_ = target_state;
    window_state->UpdateWindowShowStateFromStateType();
    window_state->NotifyPreStateTypeChange(old_state_type);
    // If we have a target bounds rectangle, we center it and set it
    // accordingly.
    if (!bounds_in_parent.IsEmpty()) {
      if (current_state_type_ == wm::WINDOW_STATE_TYPE_MINIMIZED || !animated)
        window_state->SetBoundsDirect(bounds_in_parent);
      else
        window_state->SetBoundsDirectAnimated(bounds_in_parent);
    }
    window_state->NotifyPostStateTypeChange(old_state_type);
  } else if (!bounds_in_parent.IsEmpty() &&
             bounds_in_parent != window_state->window()->bounds()) {
    // Coming here, we were most probably called through a display change.
    // Do not animate.
    if (animated)
      window_state->SetBoundsDirectAnimated(bounds_in_parent);
    else
      window_state->SetBoundsDirect(bounds_in_parent);
  }

  if ((window_state->window()->TargetVisibility() ||
      old_state_type == wm::WINDOW_STATE_TYPE_MINIMIZED) &&
      !window_state->window()->layer()->visible()) {
    // The layer may be hidden if the window was previously minimized. Make
    // sure it's visible.
    window_state->window()->Show();
  }
}

void MaximizeModeWindowState::Minimize(wm::WindowState* window_state) {
  ::wm::SetWindowVisibilityAnimationType(
      window_state->window(), WINDOW_VISIBILITY_ANIMATION_TYPE_MINIMIZE);

  // Hide the window.
  window_state->window()->Hide();
  // Activate another window.
  if (window_state->IsActive())
    window_state->Deactivate();
}

}  // namespace ash
