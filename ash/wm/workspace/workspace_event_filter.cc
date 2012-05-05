// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_event_filter.h"

#include "ash/wm/property_util.h"
#include "ash/wm/window_frame.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "ash/wm/workspace/workspace_window_resizer.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/event.h"
#include "ui/aura/window.h"
#include "ui/aura/window_delegate.h"
#include "ui/base/hit_test.h"
#include "ui/compositor/scoped_layer_animation_settings.h"
#include "ui/gfx/screen.h"

namespace {

// Sends OnWindowHoveredChanged(|hovered|) to the WindowFrame for |window|,
// which may be NULL.
void WindowHoverChanged(aura::Window* window, bool hovered) {
  if (!window)
    return;
  ash::WindowFrame* window_frame = window->GetProperty(ash::kWindowFrameKey);
  if (!window_frame)
    return;
  window_frame->OnWindowHoverChanged(hovered);
}

void SingleAxisMaximize(aura::Window* window, const gfx::Rect& maximize_rect) {
  window->ClearProperty(aura::client::kRestoreBoundsKey);
  window->SetProperty(aura::client::kRestoreBoundsKey,
                      new gfx::Rect(window->bounds()));
  window->SetBounds(maximize_rect);
}

void SingleAxisUnmaximize(aura::Window* window,
                          const gfx::Rect& restore_bounds) {
  window->SetBounds(restore_bounds);
  window->ClearProperty(aura::client::kRestoreBoundsKey);
}

}  // namespace

namespace ash {
namespace internal {

WorkspaceEventFilter::WorkspaceEventFilter(aura::Window* owner)
    : ToplevelWindowEventFilter(owner),
      hovered_window_(NULL) {
}

WorkspaceEventFilter::~WorkspaceEventFilter() {
  if (hovered_window_)
    hovered_window_->RemoveObserver(this);
}

bool WorkspaceEventFilter::PreHandleMouseEvent(aura::Window* target,
                                               aura::MouseEvent* event) {
  switch (event->type()) {
    case ui::ET_MOUSE_MOVED: {
      int component =
          target->delegate()->GetNonClientComponent(event->location());
      multi_window_resize_controller_.set_grid_size(grid_size());
      multi_window_resize_controller_.Show(target, component,
                                           event->location());
      break;
    }
    case ui::ET_MOUSE_ENTERED:
      UpdateHoveredWindow(wm::GetActivatableWindow(target));
      break;
    case ui::ET_MOUSE_CAPTURE_CHANGED:
    case ui::ET_MOUSE_EXITED:
      UpdateHoveredWindow(NULL);
      break;
    case ui::ET_MOUSE_PRESSED:
      multi_window_resize_controller_.Hide();
      HandleVerticalResizeDoubleClick(target, event);
      break;
    default:
      break;
  }
  return ToplevelWindowEventFilter::PreHandleMouseEvent(target, event);
}

void WorkspaceEventFilter::OnWindowDestroyed(aura::Window* window) {
  DCHECK_EQ(hovered_window_, window);
  hovered_window_->RemoveObserver(this);
  hovered_window_ = NULL;
}

WindowResizer* WorkspaceEventFilter::CreateWindowResizer(
    aura::Window* window,
    const gfx::Point& point,
    int window_component) {
  // Allow dragging maximized windows if it's not tracked by workspace. This is
  // set by tab dragging code.
  if (!wm::IsWindowNormal(window) &&
      (window_component != HTCAPTION || GetTrackedByWorkspace(window))) {
    return NULL;
  }
  return WorkspaceWindowResizer::Create(
      window, point, window_component,
      std::vector<aura::Window*>());
}

void WorkspaceEventFilter::UpdateHoveredWindow(
    aura::Window* toplevel_window) {
  if (toplevel_window == hovered_window_)
    return;
  if (hovered_window_)
    hovered_window_->RemoveObserver(this);
  WindowHoverChanged(hovered_window_, false);
  hovered_window_ = toplevel_window;
  WindowHoverChanged(hovered_window_, true);
  if (hovered_window_)
    hovered_window_->AddObserver(this);
}

void WorkspaceEventFilter::HandleVerticalResizeDoubleClick(
    aura::Window* target,
    aura::MouseEvent* event) {
  if (event->flags() & ui::EF_IS_DOUBLE_CLICK &&
      !wm::IsWindowMaximized(target)) {
    int component =
        target->delegate()->GetNonClientComponent(event->location());
    gfx::Rect work_area =
        gfx::Screen::GetMonitorNearestWindow(target).work_area();
    const gfx::Rect* restore_bounds =
        target->GetProperty(aura::client::kRestoreBoundsKey);
    if (component == HTBOTTOM || component == HTTOP) {
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
