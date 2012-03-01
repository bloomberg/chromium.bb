// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_event_filter.h"

#include "ash/wm/property_util.h"
#include "ash/wm/window_frame.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "ash/wm/workspace/workspace_window_resizer.h"
#include "ui/aura/event.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"

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
    case ui::ET_MOUSE_ENTERED:
      UpdateHoveredWindow(wm::GetActivatableWindow(target));
      break;
    case ui::ET_MOUSE_CAPTURE_CHANGED:
    case ui::ET_MOUSE_EXITED:
      UpdateHoveredWindow(NULL);
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
  return
      new WorkspaceWindowResizer(window, point, window_component, grid_size());
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

}  // namespace internal
}  // namespace ash
