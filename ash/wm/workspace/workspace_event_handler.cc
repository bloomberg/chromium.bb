// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_event_handler.h"

#include "ash/metrics/user_metrics_recorder.h"
#include "ash/shell.h"
#include "ash/touch/touch_uma.h"
#include "ash/wm/window_state.h"
#include "ash/wm/wm_event.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/hit_test.h"

namespace ash {

WorkspaceEventHandler::WorkspaceEventHandler()
    : click_component_(HTNOWHERE) {
}

WorkspaceEventHandler::~WorkspaceEventHandler() {
}

void WorkspaceEventHandler::OnMouseEvent(ui::MouseEvent* event) {
  aura::Window* target = static_cast<aura::Window*>(event->target());
  if (event->type() == ui::ET_MOUSE_PRESSED &&
      event->IsOnlyLeftMouseButton() &&
      ((event->flags() &
          (ui::EF_IS_DOUBLE_CLICK | ui::EF_IS_TRIPLE_CLICK)) == 0)) {
    click_component_ = target->delegate()->
        GetNonClientComponent(event->location());
  }

  if (event->handled())
    return;

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
      wm::WindowState* target_state = wm::GetWindowState(target);

      if (event->IsOnlyLeftMouseButton()) {
        if (event->flags() & ui::EF_IS_DOUBLE_CLICK) {
          int component = target->delegate()->
              GetNonClientComponent(event->location());
          if (component == HTCAPTION &&
              component == click_component_) {
            ash::Shell::GetInstance()->metrics()->RecordUserMetricsAction(
                ash::UMA_TOGGLE_MAXIMIZE_CAPTION_CLICK);
            const wm::WMEvent wm_event(wm::WM_EVENT_TOGGLE_MAXIMIZE_CAPTION);
            target_state->OnWMEvent(&wm_event);
            event->StopPropagation();
          }
          // WindowEventHandler can receive each event up to two times. Once a
          // double-click has been received clear the target. Otherwise a
          // duplicate of the event will be checking target history against
          // itself.
          click_component_ = HTNOWHERE;
        }
      } else {
        click_component_ = HTNOWHERE;
      }

      multi_window_resize_controller_.Hide();
      HandleVerticalResizeDoubleClick(target_state, event);
      break;
    }
    default:
      break;
  }
}

void WorkspaceEventHandler::OnGestureEvent(ui::GestureEvent* event) {
  if (event->handled() || event->type() != ui::ET_GESTURE_TAP)
    return;

  aura::Window* target = static_cast<aura::Window*>(event->target());
  int previous_target_component = click_component_;
  click_component_ = target->delegate()->
      GetNonClientComponent(event->location());

  if (click_component_ != HTCAPTION)
    return;

  if (event->details().tap_count() != 2) {
    // Note: TouchUMA::GESTURE_FRAMEVIEW_TAP is counted twice for each tap.
    TouchUMA::GetInstance()->
        RecordGestureAction(TouchUMA::GESTURE_FRAMEVIEW_TAP);
    return;
  }

  if (click_component_ == previous_target_component) {
    ash::Shell::GetInstance()->metrics()->RecordUserMetricsAction(
        ash::UMA_TOGGLE_MAXIMIZE_CAPTION_GESTURE);
    // Note: TouchUMA::GESTURE_FRAMEVIEW_TAP is counted twice each time
    // TouchUMA::GESTURE_MAXIMIZE_DOUBLETAP is counted once.
    TouchUMA::GetInstance()->RecordGestureAction(
        TouchUMA::GESTURE_MAXIMIZE_DOUBLETAP);
    const wm::WMEvent wm_event(wm::WM_EVENT_TOGGLE_MAXIMIZE_CAPTION);
    wm::GetWindowState(target)->OnWMEvent(&wm_event);
    event->StopPropagation();
  }
  click_component_ = HTNOWHERE;
}

void WorkspaceEventHandler::HandleVerticalResizeDoubleClick(
    wm::WindowState* target_state,
    ui::MouseEvent* event) {
  aura::Window* target = target_state->window();
  if (event->flags() & ui::EF_IS_DOUBLE_CLICK) {
    int component =
        target->delegate()->GetNonClientComponent(event->location());
    if (component == HTBOTTOM || component == HTTOP) {
      Shell::GetInstance()->metrics()->RecordUserMetricsAction(
          UMA_TOGGLE_SINGLE_AXIS_MAXIMIZE_BORDER_CLICK);
      const wm::WMEvent wm_event(wm::WM_EVENT_TOGGLE_VERTICAL_MAXIMIZE);
      target_state->OnWMEvent(&wm_event);
      event->StopPropagation();
    } else if (component == HTLEFT || component == HTRIGHT) {
      Shell::GetInstance()->metrics()->RecordUserMetricsAction(
          UMA_TOGGLE_SINGLE_AXIS_MAXIMIZE_BORDER_CLICK);
      const wm::WMEvent wm_event(wm::WM_EVENT_TOGGLE_HORIZONTAL_MAXIMIZE);
      target_state->OnWMEvent(&wm_event);
      event->StopPropagation();
    }
  }
}

}  // namespace ash
