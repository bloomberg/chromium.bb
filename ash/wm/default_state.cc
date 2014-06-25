// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/default_state.h"

#include "ash/display/display_controller.h"
#include "ash/screen_util.h"
#include "ash/shell.h"
#include "ash/shell_window_ids.h"
#include "ash/wm/coordinate_conversion.h"
#include "ash/wm/window_animations.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_state_delegate.h"
#include "ash/wm/window_state_util.h"
#include "ash/wm/window_util.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/workspace/workspace_window_resizer.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/gfx/display.h"
#include "ui/gfx/rect.h"

namespace ash {
namespace wm {
namespace {

// This specifies how much percent (30%) of a window rect
// must be visible when the window is added to the workspace.
const float kMinimumPercentOnScreenArea = 0.3f;

bool IsPanel(aura::Window* window) {
  return window->parent() &&
         window->parent()->id() == kShellWindowId_PanelContainer;
}

void MoveToDisplayForRestore(WindowState* window_state) {
  if (!window_state->HasRestoreBounds())
    return;
  const gfx::Rect& restore_bounds = window_state->GetRestoreBoundsInScreen();

  // Move only if the restore bounds is outside of
  // the display. There is no information about in which
  // display it should be restored, so this is best guess.
  // TODO(oshima): Restore information should contain the
  // work area information like WindowResizer does for the
  // last window location.
  gfx::Rect display_area = Shell::GetScreen()->GetDisplayNearestWindow(
      window_state->window()).bounds();

  if (!display_area.Intersects(restore_bounds)) {
    const gfx::Display& display =
        Shell::GetScreen()->GetDisplayMatching(restore_bounds);
    DisplayController* display_controller =
        Shell::GetInstance()->display_controller();
    aura::Window* new_root =
        display_controller->GetRootWindowForDisplayId(display.id());
    if (new_root != window_state->window()->GetRootWindow()) {
      aura::Window* new_container =
          Shell::GetContainer(new_root, window_state->window()->parent()->id());
      new_container->AddChild(window_state->window());
    }
  }
}

}  // namespace;

DefaultState::DefaultState(WindowStateType initial_state_type)
    : state_type_(initial_state_type) {}
DefaultState::~DefaultState() {}

void DefaultState::OnWMEvent(WindowState* window_state,
                             const WMEvent* event) {
  if (ProcessWorkspaceEvents(window_state, event))
    return;

  if (ProcessCompoundEvents(window_state, event))
    return;

  WindowStateType next_state_type = WINDOW_STATE_TYPE_NORMAL;
  switch (event->type()) {
    case WM_EVENT_NORMAL:
      next_state_type = WINDOW_STATE_TYPE_NORMAL;
      break;
    case WM_EVENT_MAXIMIZE:
      next_state_type = WINDOW_STATE_TYPE_MAXIMIZED;
      break;
    case WM_EVENT_MINIMIZE:
      next_state_type = WINDOW_STATE_TYPE_MINIMIZED;
      break;
    case WM_EVENT_FULLSCREEN:
      next_state_type = WINDOW_STATE_TYPE_FULLSCREEN;
      break;
    case WM_EVENT_SNAP_LEFT:
      next_state_type = WINDOW_STATE_TYPE_LEFT_SNAPPED;
      break;
    case WM_EVENT_SNAP_RIGHT:
      next_state_type = WINDOW_STATE_TYPE_RIGHT_SNAPPED;
      break;
    case WM_EVENT_SET_BOUNDS:
      SetBounds(window_state, static_cast<const SetBoundsEvent*>(event));
      return;
    case WM_EVENT_SHOW_INACTIVE:
      next_state_type = WINDOW_STATE_TYPE_INACTIVE;
      break;
    case WM_EVENT_TOGGLE_MAXIMIZE_CAPTION:
    case WM_EVENT_TOGGLE_MAXIMIZE:
    case WM_EVENT_TOGGLE_VERTICAL_MAXIMIZE:
    case WM_EVENT_TOGGLE_HORIZONTAL_MAXIMIZE:
    case WM_EVENT_TOGGLE_FULLSCREEN:
    case WM_EVENT_CENTER:
      NOTREACHED() << "Compound event should not reach here:" << event;
      return;
    case WM_EVENT_ADDED_TO_WORKSPACE:
    case WM_EVENT_WORKAREA_BOUNDS_CHANGED:
    case WM_EVENT_DISPLAY_BOUNDS_CHANGED:
      NOTREACHED() << "Workspace event should not reach here:" << event;
      return;
  }

  WindowStateType current = window_state->GetStateType();

  if (next_state_type == current && window_state->IsSnapped()) {
    gfx::Rect snapped_bounds = event->type() == WM_EVENT_SNAP_LEFT ?
        GetDefaultLeftSnappedWindowBoundsInParent(window_state->window()) :
        GetDefaultRightSnappedWindowBoundsInParent(window_state->window());
    window_state->SetBoundsDirectAnimated(snapped_bounds);
    return;
  }

  EnterToNextState(window_state, next_state_type);
}

WindowStateType DefaultState::GetType() const {
  return state_type_;
}

void DefaultState::AttachState(
    WindowState* window_state,
    WindowState::State* state_in_previous_mode) {
  DCHECK_EQ(stored_window_state_, window_state);

  ReenterToCurrentState(window_state, state_in_previous_mode);

  // If the display has changed while in the another mode,
  // we need to let windows know the change.
  gfx::Display current_display = Shell::GetScreen()->
      GetDisplayNearestWindow(window_state->window());
  if (stored_display_state_.bounds() != current_display.bounds()) {
    const WMEvent event(wm::WM_EVENT_DISPLAY_BOUNDS_CHANGED);
    window_state->OnWMEvent(&event);
  } else if (stored_display_state_.work_area() != current_display.work_area()) {
    const WMEvent event(wm::WM_EVENT_WORKAREA_BOUNDS_CHANGED);
    window_state->OnWMEvent(&event);
  }
}

void DefaultState::DetachState(WindowState* window_state) {
  stored_window_state_ = window_state;
  aura::Window* window = window_state->window();
  stored_bounds_ = window->bounds();
  stored_restore_bounds_ = window_state->HasRestoreBounds() ?
      window_state->GetRestoreBoundsInParent() : gfx::Rect();
  // Remember the display state so that in case of the display change
  // while in the other mode, we can perform necessary action to
  // restore the window state to the proper state for the current
  // display.
  stored_display_state_ = Shell::GetScreen()->
      GetDisplayNearestWindow(window_state->window());
}

// static
bool DefaultState::ProcessCompoundEvents(WindowState* window_state,
                                         const WMEvent* event) {
  aura::Window* window = window_state->window();

  switch (event->type()) {
    case WM_EVENT_TOGGLE_MAXIMIZE_CAPTION:
      if (window_state->IsFullscreen()) {
        const wm::WMEvent event(wm::WM_EVENT_TOGGLE_FULLSCREEN);
        window_state->OnWMEvent(&event);
      } else if (window_state->IsMaximized()) {
        window_state->Restore();
      } else if (window_state->IsNormalOrSnapped()) {
        if (window_state->CanMaximize())
          window_state->Maximize();
      }
      return true;
    case WM_EVENT_TOGGLE_MAXIMIZE:
      if (window_state->IsFullscreen()) {
        const wm::WMEvent event(wm::WM_EVENT_TOGGLE_FULLSCREEN);
        window_state->OnWMEvent(&event);
      } else if (window_state->IsMaximized()) {
        window_state->Restore();
      } else if (window_state->CanMaximize()) {
        window_state->Maximize();
      }
      return true;
    case WM_EVENT_TOGGLE_VERTICAL_MAXIMIZE: {
      gfx::Rect work_area =
          ScreenUtil::GetDisplayWorkAreaBoundsInParent(window);

      // Maximize vertically if:
      // - The window does not have a max height defined.
      // - The window has the normal state type. Snapped windows are excluded
      //   because they are already maximized vertically and reverting to the
      //   restored bounds looks weird.
      if (window->delegate()->GetMaximumSize().height() != 0 ||
          !window_state->IsNormalStateType()) {
        return true;
      }
      if (window_state->HasRestoreBounds() &&
          (window->bounds().height() == work_area.height() &&
           window->bounds().y() == work_area.y())) {
        window_state->SetAndClearRestoreBounds();
      } else {
        window_state->SaveCurrentBoundsForRestore();
        window->SetBounds(gfx::Rect(window->bounds().x(),
                                    work_area.y(),
                                    window->bounds().width(),
                                    work_area.height()));
      }
      return true;
    }
    case WM_EVENT_TOGGLE_HORIZONTAL_MAXIMIZE: {
      // Maximize horizontally if:
      // - The window does not have a max width defined.
      // - The window is snapped or has the normal state type.
      if (window->delegate()->GetMaximumSize().width() != 0)
        return true;
      if (!window_state->IsNormalOrSnapped())
        return true;
      gfx::Rect work_area =
          ScreenUtil::GetDisplayWorkAreaBoundsInParent(window);
      if (window_state->IsNormalStateType() &&
          window_state->HasRestoreBounds() &&
          (window->bounds().width() == work_area.width() &&
           window->bounds().x() == work_area.x())) {
        window_state->SetAndClearRestoreBounds();
      } else {
        gfx::Rect new_bounds(work_area.x(),
                             window->bounds().y(),
                             work_area.width(),
                             window->bounds().height());

        gfx::Rect restore_bounds = window->bounds();
        if (window_state->IsSnapped()) {
          window_state->SetRestoreBoundsInParent(new_bounds);
          window_state->Restore();

          // The restore logic prevents a window from being restored to bounds
          // which match the workspace bounds exactly so it is necessary to set
          // the bounds again below.
        }

        window_state->SetRestoreBoundsInParent(restore_bounds);
        window->SetBounds(new_bounds);
      }
      return true;
    }
    case WM_EVENT_TOGGLE_FULLSCREEN:
      ToggleFullScreen(window_state, window_state->delegate());
      return true;
    case WM_EVENT_CENTER:
      CenterWindow(window_state);
      return true;
    case WM_EVENT_NORMAL:
    case WM_EVENT_MAXIMIZE:
    case WM_EVENT_MINIMIZE:
    case WM_EVENT_FULLSCREEN:
    case WM_EVENT_SNAP_LEFT:
    case WM_EVENT_SNAP_RIGHT:
    case WM_EVENT_SET_BOUNDS:
    case WM_EVENT_SHOW_INACTIVE:
      break;
    case WM_EVENT_ADDED_TO_WORKSPACE:
    case WM_EVENT_WORKAREA_BOUNDS_CHANGED:
    case WM_EVENT_DISPLAY_BOUNDS_CHANGED:
      NOTREACHED() << "Workspace event should not reach here:" << event;
      break;
  }
  return false;
}

bool DefaultState::ProcessWorkspaceEvents(WindowState* window_state,
                                          const WMEvent* event) {
  switch (event->type()) {
    case WM_EVENT_ADDED_TO_WORKSPACE: {
      // When a window is dragged and dropped onto a different
      // root window, the bounds will be updated after they are added
      // to the root window.
      // If a window is opened as maximized or fullscreen, its bounds may be
      // empty, so update the bounds now before checking empty.
      if (window_state->is_dragged() ||
          SetMaximizedOrFullscreenBounds(window_state)) {
        return true;
      }

      aura::Window* window = window_state->window();
      gfx::Rect bounds = window->bounds();

      // Don't adjust window bounds if the bounds are empty as this
      // happens when a new views::Widget is created.
      if (bounds.IsEmpty())
        return true;

      // Only windows of type WINDOW_TYPE_NORMAL or WINDOW_TYPE_PANEL need to be
      // adjusted to have minimum visibility, because they are positioned by the
      // user and user should always be able to interact with them. Other
      // windows are positioned programmatically.
      if (window_state->window()->type() != ui::wm::WINDOW_TYPE_NORMAL &&
          window_state->window()->type() != ui::wm::WINDOW_TYPE_PANEL) {
        return true;
      }

      // Use entire display instead of workarea because the workarea can
      // be further shrunk by the docked area. The logic ensures 30%
      // visibility which should be enough to see where the window gets
      // moved.
      gfx::Rect display_area = ScreenUtil::GetDisplayBoundsInParent(window);
      int min_width = bounds.width() * kMinimumPercentOnScreenArea;
      int min_height = bounds.height() * kMinimumPercentOnScreenArea;
      AdjustBoundsToEnsureWindowVisibility(
          display_area, min_width, min_height, &bounds);
      window_state->AdjustSnappedBounds(&bounds);
      if (window->bounds() != bounds)
        window_state->SetBoundsConstrained(bounds);
      return true;
    }
    case WM_EVENT_DISPLAY_BOUNDS_CHANGED: {
      if (window_state->is_dragged() ||
          SetMaximizedOrFullscreenBounds(window_state)) {
        return true;
      }
      gfx::Rect work_area_in_parent =
          ScreenUtil::GetDisplayWorkAreaBoundsInParent(window_state->window());
      gfx::Rect bounds = window_state->window()->bounds();
      // When display bounds has changed, make sure the entire window is fully
      // visible.
      bounds.AdjustToFit(work_area_in_parent);
      window_state->AdjustSnappedBounds(&bounds);
      if (window_state->window()->bounds() != bounds)
        window_state->SetBoundsDirectAnimated(bounds);
      return true;
    }
    case WM_EVENT_WORKAREA_BOUNDS_CHANGED: {
      if (window_state->is_dragged() ||
          SetMaximizedOrFullscreenBounds(window_state)) {
        return true;
      }
      gfx::Rect work_area_in_parent =
          ScreenUtil::GetDisplayWorkAreaBoundsInParent(window_state->window());
      gfx::Rect bounds = window_state->window()->bounds();
      AdjustBoundsToEnsureMinimumWindowVisibility(work_area_in_parent, &bounds);
      window_state->AdjustSnappedBounds(&bounds);
      if (window_state->window()->bounds() != bounds)
        window_state->SetBoundsDirectAnimated(bounds);
      return true;
    }
    case WM_EVENT_TOGGLE_MAXIMIZE_CAPTION:
    case WM_EVENT_TOGGLE_MAXIMIZE:
    case WM_EVENT_TOGGLE_VERTICAL_MAXIMIZE:
    case WM_EVENT_TOGGLE_HORIZONTAL_MAXIMIZE:
    case WM_EVENT_TOGGLE_FULLSCREEN:
    case WM_EVENT_CENTER:
    case WM_EVENT_NORMAL:
    case WM_EVENT_MAXIMIZE:
    case WM_EVENT_MINIMIZE:
    case WM_EVENT_FULLSCREEN:
    case WM_EVENT_SNAP_LEFT:
    case WM_EVENT_SNAP_RIGHT:
    case WM_EVENT_SET_BOUNDS:
    case WM_EVENT_SHOW_INACTIVE:
      break;
  }
  return false;
}

// static
bool DefaultState::SetMaximizedOrFullscreenBounds(WindowState* window_state) {
  DCHECK(!window_state->is_dragged());
  if (window_state->IsMaximized()) {
    window_state->SetBoundsDirect(
        ScreenUtil::GetMaximizedWindowBoundsInParent(window_state->window()));
    return true;
  }
  if (window_state->IsFullscreen()) {
    window_state->SetBoundsDirect(
        ScreenUtil::GetDisplayBoundsInParent(window_state->window()));
    return true;
  }
  return false;
}

// static
void DefaultState::SetBounds(WindowState* window_state,
                             const SetBoundsEvent* event) {
  if (window_state->is_dragged()) {
    window_state->SetBoundsDirect(event->requested_bounds());
  } else if (window_state->IsSnapped()) {
    gfx::Rect work_area_in_parent =
        ScreenUtil::GetDisplayWorkAreaBoundsInParent(window_state->window());
    gfx::Rect child_bounds(event->requested_bounds());
    AdjustBoundsSmallerThan(work_area_in_parent.size(), &child_bounds);
    window_state->AdjustSnappedBounds(&child_bounds);
    window_state->SetBoundsDirect(child_bounds);
  } else if (!SetMaximizedOrFullscreenBounds(window_state)) {
    window_state->SetBoundsConstrained(event->requested_bounds());
  }
}

void DefaultState::EnterToNextState(WindowState* window_state,
                                    WindowStateType next_state_type) {
  // Do nothing if  we're already in the same state.
  if (state_type_ == next_state_type)
    return;

  WindowStateType previous_state_type = state_type_;
  state_type_ = next_state_type;

  window_state->UpdateWindowShowStateFromStateType();
  window_state->NotifyPreStateTypeChange(previous_state_type);

  // This Docked/Snapped hack is due to the issue that IsDocked returns
  // true for dragging window.  TODO(oshima): Make docked window a state
  // and remove this hack.
  if (window_state->window()->parent() &&
      (window_state->IsSnapped() ||
       (!window_state->IsDocked() && !IsPanel(window_state->window())))) {
    if (!window_state->HasRestoreBounds() &&
        (previous_state_type == WINDOW_STATE_TYPE_DEFAULT ||
         previous_state_type == WINDOW_STATE_TYPE_NORMAL) &&
        !window_state->IsMinimized() &&
        !window_state->IsNormalStateType()) {
      window_state->SaveCurrentBoundsForRestore();
    }

    // When restoring from a minimized state, we want to restore to the previous
    // bounds. However, we want to maintain the restore bounds. (The restore
    // bounds are set if a user maximized the window in one axis by double
    // clicking the window border for example).
    gfx::Rect restore_bounds_in_screen;
    if (previous_state_type == WINDOW_STATE_TYPE_MINIMIZED &&
        window_state->IsNormalStateType() &&
        window_state->HasRestoreBounds() &&
        !window_state->unminimize_to_restore_bounds()) {
      restore_bounds_in_screen = window_state->GetRestoreBoundsInScreen();
      window_state->SaveCurrentBoundsForRestore();
    }

    if (window_state->IsMaximizedOrFullscreen())
      MoveToDisplayForRestore(window_state);

    UpdateBoundsFromState(window_state, previous_state_type);

    // Normal state should have no restore bounds unless it's
    // unminimzied.
    if (!restore_bounds_in_screen.IsEmpty())
      window_state->SetRestoreBoundsInScreen(restore_bounds_in_screen);
    else if (window_state->IsNormalStateType())
      window_state->ClearRestoreBounds();
  }
  window_state->NotifyPostStateTypeChange(previous_state_type);
}

void DefaultState::ReenterToCurrentState(
    WindowState* window_state,
    WindowState::State* state_in_previous_mode) {
  WindowStateType previous_state_type = state_in_previous_mode->GetType();
  if (previous_state_type == wm::WINDOW_STATE_TYPE_FULLSCREEN) {
    // A state change should not move a window out of full screen since full
    // screen is a "special mode" the user wanted to be in and should be
    // respected as such.
    state_type_ = wm::WINDOW_STATE_TYPE_FULLSCREEN;
  }
  window_state->UpdateWindowShowStateFromStateType();
  window_state->NotifyPreStateTypeChange(previous_state_type);

  if ((state_type_ == wm::WINDOW_STATE_TYPE_NORMAL ||
       state_type_ == wm::WINDOW_STATE_TYPE_DEFAULT) &&
      !stored_bounds_.IsEmpty()) {
    // Use the restore mechanism to set the bounds for
    // the window in normal state. This also covers unminimize case.
    window_state->SetRestoreBoundsInParent(stored_bounds_);
  }

  UpdateBoundsFromState(window_state, state_in_previous_mode->GetType());

  // Then restore the restore bounds to their previous value.
  if (!stored_restore_bounds_.IsEmpty())
    window_state->SetRestoreBoundsInParent(stored_restore_bounds_);
  else
    window_state->ClearRestoreBounds();

  window_state->NotifyPostStateTypeChange(previous_state_type);
}

void DefaultState::UpdateBoundsFromState(WindowState* window_state,
                                         WindowStateType previous_state_type) {
  aura::Window* window = window_state->window();
  gfx::Rect bounds_in_parent;
  switch (state_type_) {
    case WINDOW_STATE_TYPE_LEFT_SNAPPED:
    case WINDOW_STATE_TYPE_RIGHT_SNAPPED:
      bounds_in_parent = state_type_ == WINDOW_STATE_TYPE_LEFT_SNAPPED ?
          GetDefaultLeftSnappedWindowBoundsInParent(window_state->window()) :
          GetDefaultRightSnappedWindowBoundsInParent(window_state->window());
      break;
    case WINDOW_STATE_TYPE_DEFAULT:
    case WINDOW_STATE_TYPE_NORMAL: {
      gfx::Rect work_area_in_parent =
          ScreenUtil::GetDisplayWorkAreaBoundsInParent(window_state->window());
      if (window_state->HasRestoreBounds())
        bounds_in_parent = window_state->GetRestoreBoundsInParent();
      else
        bounds_in_parent = window->bounds();
      // Make sure that part of the window is always visible.
      AdjustBoundsToEnsureMinimumWindowVisibility(
          work_area_in_parent, &bounds_in_parent);
      break;
    }
    case WINDOW_STATE_TYPE_MAXIMIZED:
      bounds_in_parent = ScreenUtil::GetMaximizedWindowBoundsInParent(window);
      break;

    case WINDOW_STATE_TYPE_FULLSCREEN:
      bounds_in_parent = ScreenUtil::GetDisplayBoundsInParent(window);
      break;

    case WINDOW_STATE_TYPE_MINIMIZED:
      break;
    case WINDOW_STATE_TYPE_INACTIVE:
    case WINDOW_STATE_TYPE_DETACHED:
    case WINDOW_STATE_TYPE_END:
    case WINDOW_STATE_TYPE_AUTO_POSITIONED:
      return;
  }

  if (state_type_ != WINDOW_STATE_TYPE_MINIMIZED) {
    if (previous_state_type == WINDOW_STATE_TYPE_MINIMIZED ||
        window_state->IsFullscreen()) {
      window_state->SetBoundsDirect(bounds_in_parent);
    } else if (window_state->IsMaximized() ||
               IsMaximizedOrFullscreenWindowStateType(previous_state_type)) {
      window_state->SetBoundsDirectCrossFade(bounds_in_parent);
    } else if (window_state->is_dragged()) {
      // SetBoundsDirectAnimated does not work when the window gets reparented.
      // TODO(oshima): Consider fixing it and reenable the animation.
      window_state->SetBoundsDirect(bounds_in_parent);
    } else {
      window_state->SetBoundsDirectAnimated(bounds_in_parent);
    }
  }

  if (window_state->IsMinimized()) {
    // Save the previous show state so that we can correctly restore it.
    window_state->window()->SetProperty(aura::client::kRestoreShowStateKey,
                                        ToWindowShowState(previous_state_type));
    ::wm::SetWindowVisibilityAnimationType(
        window_state->window(), WINDOW_VISIBILITY_ANIMATION_TYPE_MINIMIZE);

    // Hide the window.
    window_state->window()->Hide();
    // Activate another window.
    if (window_state->IsActive())
      window_state->Deactivate();
  } else if ((window_state->window()->TargetVisibility() ||
              previous_state_type == WINDOW_STATE_TYPE_MINIMIZED) &&
             !window_state->window()->layer()->visible()) {
    // The layer may be hidden if the window was previously minimized. Make
    // sure it's visible.
    window_state->window()->Show();
    if (previous_state_type == WINDOW_STATE_TYPE_MINIMIZED &&
        !window_state->IsMaximizedOrFullscreen()) {
      window_state->set_unminimize_to_restore_bounds(false);
    }
  }
}

// static
void DefaultState::CenterWindow(WindowState* window_state) {
  if (!window_state->IsNormalOrSnapped())
    return;
  aura::Window* window = window_state->window();
  if (window_state->IsSnapped()) {
    gfx::Rect center_in_screen =
        Shell::GetScreen()->GetDisplayNearestWindow(window).work_area();
    gfx::Size size = window_state->HasRestoreBounds() ?
        window_state->GetRestoreBoundsInScreen().size() :
        window->bounds().size();
    center_in_screen.ClampToCenteredSize(size);
    window_state->SetRestoreBoundsInScreen(center_in_screen);
    window_state->Restore();
  } else {
    gfx::Rect center_in_parent =
        ScreenUtil::GetDisplayWorkAreaBoundsInParent(window);
    center_in_parent.ClampToCenteredSize(window->bounds().size());
    window_state->SetBoundsDirectAnimated(center_in_parent);
  }
  // Centering window is treated as if a user moved and resized the window.
  window_state->set_bounds_changed_by_user(true);
}

}  // namespace wm
}  // namespace ash
