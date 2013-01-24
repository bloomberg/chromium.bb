// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_event_handler.h"

#include "ash/screen_ash.h"
#include "ash/shell.h"
#include "ash/shell_delegate.h"
#include "ash/wm/property_util.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/workspace_window_resizer.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/events/event.h"
#include "ui/base/events/event_utils.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/screen.h"

namespace ash {
namespace {

void SingleAxisMaximize(aura::Window* window, const gfx::Rect& maximize_rect) {
  gfx::Rect bounds_in_screen =
      ScreenAsh::ConvertRectToScreen(window->parent(), window->bounds());

  SetRestoreBoundsInScreen(window, bounds_in_screen);
  window->SetBounds(maximize_rect);
}

void SingleAxisUnmaximize(aura::Window* window,
                          const gfx::Rect& restore_bounds_in_screen) {
  gfx::Rect restore_bounds = ScreenAsh::ConvertRectFromScreen(
      window->parent(), restore_bounds_in_screen);
  window->SetBounds(restore_bounds);
  ClearRestoreBounds(window);
}

void ToggleMaximizedState(aura::Window* window) {
  if (GetRestoreBoundsInScreen(window)) {
    if (window->GetProperty(aura::client::kShowStateKey) ==
        ui::SHOW_STATE_NORMAL) {
      window->SetBounds(GetRestoreBoundsInParent(window));
      ClearRestoreBounds(window);
    } else {
      window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_NORMAL);
    }
  } else if (wm::CanMaximizeWindow(window)) {
    window->SetProperty(aura::client::kShowStateKey, ui::SHOW_STATE_MAXIMIZED);
  }
}

}  // namespace

namespace internal {

WorkspaceEventHandler::WorkspaceEventHandler(aura::Window* owner)
    : ToplevelWindowEventHandler(owner),
      destroyed_(NULL) {
}

WorkspaceEventHandler::~WorkspaceEventHandler() {
  if (destroyed_)
    *destroyed_ = true;
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

      if (event->flags() & ui::EF_IS_DOUBLE_CLICK &&
          target->delegate()->GetNonClientComponent(event->location()) ==
          HTCAPTION) {
        bool destroyed = false;
        destroyed_ = &destroyed;
        ash::Shell::GetInstance()->delegate()->RecordUserMetricsAction(
            ash::UMA_TOGGLE_MAXIMIZE_CAPTION_CLICK);
        ToggleMaximizedState(target);
        if (destroyed)
          return;
        destroyed_ = NULL;
      }
      multi_window_resize_controller_.Hide();
      HandleVerticalResizeDoubleClick(target, event);
      break;
    }
    default:
      break;
  }
  ToplevelWindowEventHandler::OnMouseEvent(event);
}

void WorkspaceEventHandler::OnGestureEvent(ui::GestureEvent* event) {
  aura::Window* target = static_cast<aura::Window*>(event->target());
  if (event->type() == ui::ET_GESTURE_DOUBLE_TAP &&
      target->delegate()->GetNonClientComponent(event->location()) ==
      HTCAPTION) {
    ash::Shell::GetInstance()->delegate()->RecordUserMetricsAction(
        ash::UMA_TOGGLE_MAXIMIZE_CAPTION_GESTURE);
    ToggleMaximizedState(target);  // |this| may be destroyed from here.
    event->StopPropagation();
    return;
  }
  ToplevelWindowEventHandler::OnGestureEvent(event);
}

void WorkspaceEventHandler::HandleVerticalResizeDoubleClick(
    aura::Window* target,
    ui::MouseEvent* event) {
  gfx::Rect max_size(target->delegate()->GetMaximumSize());
  if (event->flags() & ui::EF_IS_DOUBLE_CLICK &&
      !wm::IsWindowMaximized(target)) {
    int component =
        target->delegate()->GetNonClientComponent(event->location());
    gfx::Rect work_area =
        Shell::GetScreen()->GetDisplayNearestWindow(target).work_area();
    const gfx::Rect* restore_bounds =
        GetRestoreBoundsInScreen(target);
    if (component == HTBOTTOM || component == HTTOP) {
      // Don't maximize vertically if the window has a max height defined.
      if (max_size.height() != 0)
        return;
      if (restore_bounds &&
          (target->bounds().height() == work_area.height() &&
           target->bounds().y() == work_area.y())) {
        SingleAxisUnmaximize(target, *restore_bounds);
      } else {
        SingleAxisMaximize(target,
                           gfx::Rect(target->bounds().x(),
                                     work_area.y(),
                                     target->bounds().width(),
                                     work_area.height()));
      }
    } else if (component == HTLEFT || component == HTRIGHT) {
      // Don't maximize horizontally if the window has a max width defined.
      if (max_size.width() != 0)
        return;
      if (restore_bounds &&
          (target->bounds().width() == work_area.width() &&
           target->bounds().x() == work_area.x())) {
        SingleAxisUnmaximize(target, *restore_bounds);
      } else {
        SingleAxisMaximize(target,
                           gfx::Rect(work_area.x(),
                                     target->bounds().y(),
                                     work_area.width(),
                                     target->bounds().height()));
      }
    }
  }
}

}  // namespace internal
}  // namespace ash
