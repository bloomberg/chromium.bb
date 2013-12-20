// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_event_handler.h"

#include "ash/metrics/user_metrics_recorder.h"
#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/touch/touch_uma.h"
#include "ash/wm/coordinate_conversion.h"
#include "ash/wm/window_state.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/workspace_window_resizer.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/screen.h"

namespace ash {
namespace {

void SingleAxisMaximize(wm::WindowState* window_state,
                        const gfx::Rect& maximize_rect_in_screen) {
  window_state->SaveCurrentBoundsForRestore();
  window_state->SetBoundsInScreen(maximize_rect_in_screen);
}

void SingleAxisUnmaximize(wm::WindowState* window_state,
                          const gfx::Rect& restore_bounds_in_screen) {
  window_state->SetBoundsInScreen(restore_bounds_in_screen);
  window_state->ClearRestoreBounds();
}

void ToggleMaximizedState(wm::WindowState* window_state) {
  if (window_state->HasRestoreBounds()) {
    if (window_state->GetShowState() == ui::SHOW_STATE_NORMAL) {
      window_state->window()->SetBounds(
          window_state->GetRestoreBoundsInParent());
      window_state->ClearRestoreBounds();
    } else {
      window_state->Restore();
    }
  } else if (window_state->CanMaximize()) {
    window_state->Maximize();
  }
}

}  // namespace

namespace internal {

WorkspaceEventHandler::WorkspaceEventHandler(aura::Window* owner)
    : ToplevelWindowEventHandler(owner) {
}

WorkspaceEventHandler::~WorkspaceEventHandler() {
}

void WorkspaceEventHandler::OnMouseEvent(ui::MouseEvent* event) {
  aura::Window* target = static_cast<aura::Window*>(event->target());
  switch (event->type()) {
    case ui::ET_MOUSE_MOVED: {
      int component =
          target->delegate()->GetNonClientComponent(event->location());
      multi_window_resize_controller_.Show(target, component,
                                           event->location());
      break;
    }
    case ui::ET_MOUSE_ENTERED:
      break;
    case ui::ET_MOUSE_CAPTURE_CHANGED:
    case ui::ET_MOUSE_EXITED:
      break;
    case ui::ET_MOUSE_PRESSED: {
      // Maximize behavior is implemented as post-target handling so the target
      // can cancel it.
      if (ui::EventCanceledDefaultHandling(*event)) {
        ToplevelWindowEventHandler::OnMouseEvent(event);
        return;
      }
      wm::WindowState* target_state = wm::GetWindowState(target);
      if (event->flags() & ui::EF_IS_DOUBLE_CLICK &&
          event->IsOnlyLeftMouseButton() &&
          target->delegate()->GetNonClientComponent(event->location()) ==
          HTCAPTION) {
        ash::Shell::GetInstance()->metrics()->RecordUserMetricsAction(
            ash::UMA_TOGGLE_MAXIMIZE_CAPTION_CLICK);
        ToggleMaximizedState(target_state);
      }
      multi_window_resize_controller_.Hide();
      HandleVerticalResizeDoubleClick(target_state, event);
      break;
    }
    default:
      break;
  }
  ToplevelWindowEventHandler::OnMouseEvent(event);
}

void WorkspaceEventHandler::OnGestureEvent(ui::GestureEvent* event) {
  aura::Window* target = static_cast<aura::Window*>(event->target());
  if (event->type() == ui::ET_GESTURE_TAP &&
      target->delegate()->GetNonClientComponent(event->location()) ==
      HTCAPTION) {
    if (event->details().tap_count() == 2) {
      ash::Shell::GetInstance()->metrics()->RecordUserMetricsAction(
          ash::UMA_TOGGLE_MAXIMIZE_CAPTION_GESTURE);
      // Note: TouchUMA::GESTURE_FRAMEVIEW_TAP is counted twice each time
      // TouchUMA::GESTURE_MAXIMIZE_DOUBLETAP is counted once.
      TouchUMA::GetInstance()->RecordGestureAction(
          TouchUMA::GESTURE_MAXIMIZE_DOUBLETAP);
      ToggleMaximizedState(wm::GetWindowState(target));
      event->StopPropagation();
      return;
    } else {
      // Note: TouchUMA::GESTURE_FRAMEVIEW_TAP is counted twice for each tap.
      TouchUMA::GetInstance()->RecordGestureAction(
          TouchUMA::GESTURE_FRAMEVIEW_TAP);
    }
  }
  ToplevelWindowEventHandler::OnGestureEvent(event);
}

void WorkspaceEventHandler::HandleVerticalResizeDoubleClick(
    wm::WindowState* target_state,
    ui::MouseEvent* event) {
  aura::Window* target = target_state->window();
  gfx::Rect max_size(target->delegate()->GetMaximumSize());
  if (event->flags() & ui::EF_IS_DOUBLE_CLICK && !target_state->IsMaximized()) {
    int component =
        target->delegate()->GetNonClientComponent(event->location());
    gfx::Rect work_area = Shell::GetScreen()->GetDisplayNearestWindow(
        target).work_area();
    if (component == HTBOTTOM || component == HTTOP) {
      // Don't maximize vertically if the window has a max height defined.
      if (max_size.height() != 0)
        return;
      if (target_state->HasRestoreBounds() &&
          (target->bounds().height() == work_area.height() &&
           target->bounds().y() == work_area.y())) {
        SingleAxisUnmaximize(target_state,
                             target_state->GetRestoreBoundsInScreen());
      } else {
        gfx::Point origin = target->bounds().origin();
        wm::ConvertPointToScreen(target->parent(), &origin);
        SingleAxisMaximize(target_state,
                           gfx::Rect(origin.x(),
                                     work_area.y(),
                                     target->bounds().width(),
                                     work_area.height()));
      }
    } else if (component == HTLEFT || component == HTRIGHT) {
      // Don't maximize horizontally if the window has a max width defined.
      if (max_size.width() != 0)
        return;
      if (target_state->HasRestoreBounds() &&
          (target->bounds().width() == work_area.width() &&
           target->bounds().x() == work_area.x())) {
        SingleAxisUnmaximize(target_state,
                             target_state->GetRestoreBoundsInScreen());
      } else {
        gfx::Point origin = target->bounds().origin();
        wm::ConvertPointToScreen(target->parent(), &origin);
        SingleAxisMaximize(target_state,
                           gfx::Rect(work_area.x(),
                                     origin.y(),
                                     work_area.width(),
                                     target->bounds().height()));
      }
    }
  }
}

}  // namespace internal
}  // namespace ash
