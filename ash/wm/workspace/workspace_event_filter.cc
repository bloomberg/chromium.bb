// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/workspace/workspace_event_filter.h"

#include "ash/wm/window_frame.h"
#include "ash/wm/window_util.h"
#include "ash/wm/workspace/workspace_layout_manager.h"
#include "ui/aura/event.h"
#include "ui/aura/window.h"
#include "ui/base/hit_test.h"

namespace {

// Sends OnWindowHoveredChanged(|hovered|) to the WindowFrame for |window|,
// which may be NULL.
void WindowHoverChanged(aura::Window* window, bool hovered) {
  if (!window)
    return;
  ash::WindowFrame* window_frame =
      static_cast<ash::WindowFrame*>(
          window->GetProperty(ash::kWindowFrameKey));
  if (!window_frame)
    return;
  window_frame->OnWindowHoverChanged(hovered);
}

}  // namespace

namespace ash {
namespace internal {

WorkspaceEventFilter::WorkspaceEventFilter(aura::Window* owner)
    : ToplevelWindowEventFilter(owner),
      drag_state_(DRAG_NONE),
      hovered_window_(NULL) {
}

WorkspaceEventFilter::~WorkspaceEventFilter() {
}

bool WorkspaceEventFilter::PreHandleMouseEvent(aura::Window* target,
                                               aura::MouseEvent* event) {
  WorkspaceLayoutManager* layout_manager =
      static_cast<WorkspaceLayoutManager*>(owner()->layout_manager());
  DCHECK(layout_manager);

  // TODO(oshima|derat): Incorporate the logic below and introduce DragObserver
  // (or something similar) to decouple DCLM.

  // Notify layout manager that drag event may move/resize the target wnidow.
  if (event->type() == ui::ET_MOUSE_DRAGGED && drag_state_ == DRAG_NONE)
    layout_manager->PrepareForMoveOrResize(target, event);

  bool handled = ToplevelWindowEventFilter::PreHandleMouseEvent(target, event);

  switch (event->type()) {
    case ui::ET_MOUSE_DRAGGED:
      // Cancel move/resize if the event wasn't handled, or
      // drag_state_ didn't move to MOVE or RESIZE.
      if (handled) {
        switch (drag_state_) {
          case DRAG_NONE:
            if (!UpdateDragState())
              layout_manager->CancelMoveOrResize(target, event);
            break;
          case DRAG_MOVE:
            layout_manager->ProcessMove(target, event);
            break;
          case DRAG_RESIZE:
            break;
        }
      } else {
        layout_manager->CancelMoveOrResize(target, event);
      }
      break;
    case ui::ET_MOUSE_ENTERED:
      UpdateHoveredWindow(GetActivatableWindow(target));
      break;
    case ui::ET_MOUSE_EXITED:
      UpdateHoveredWindow(NULL);
      break;
    case ui::ET_MOUSE_RELEASED:
      if (drag_state_ == DRAG_MOVE)
        layout_manager->EndMove(target, event);
      else if (drag_state_ == DRAG_RESIZE)
        layout_manager->EndResize(target, event);

      drag_state_ = DRAG_NONE;
      break;
    default:
      break;
  }
  return handled;
}

bool WorkspaceEventFilter::UpdateDragState() {
  DCHECK_EQ(DRAG_NONE, drag_state_);
  switch (window_component()) {
    case HTCAPTION:
      drag_state_ = DRAG_MOVE;
      break;
    case HTTOP:
    case HTTOPRIGHT:
    case HTRIGHT:
    case HTBOTTOMRIGHT:
    case HTBOTTOM:
    case HTBOTTOMLEFT:
    case HTLEFT:
    case HTTOPLEFT:
    case HTGROWBOX:
      drag_state_ = DRAG_RESIZE;
      break;
    default:
      return false;
  }
  return true;
}

void WorkspaceEventFilter::UpdateHoveredWindow(
    aura::Window* toplevel_window) {
  if (toplevel_window == hovered_window_)
    return;
  WindowHoverChanged(hovered_window_, false);
  hovered_window_ = toplevel_window;
  WindowHoverChanged(hovered_window_, true);
}

}  // namespace internal
}  // namespace ash
