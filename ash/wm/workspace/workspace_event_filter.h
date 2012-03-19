// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_WORKSPACE_EVENT_FILTER_H_
#define ASH_WM_WORKSPACE_WORKSPACE_EVENT_FILTER_H_
#pragma once

#include "ash/wm/toplevel_window_event_filter.h"
#include "ash/wm/workspace/multi_window_resize_controller.h"
#include "ui/aura/window_observer.h"

namespace aura {
class MouseEvent;
class Window;
}

namespace ash {
namespace internal {

class WorkspaceEventFilterTestHelper;

class WorkspaceEventFilter : public ToplevelWindowEventFilter,
                             public aura::WindowObserver {
 public:
  explicit WorkspaceEventFilter(aura::Window* owner);
  virtual ~WorkspaceEventFilter();

  // Overridden from ToplevelWindowEventFilter:
  virtual bool PreHandleMouseEvent(aura::Window* target,
                                   aura::MouseEvent* event) OVERRIDE;

  // Overridden from WindowObserver:
  virtual void OnWindowDestroyed(aura::Window* window) OVERRIDE;

 protected:
  // Overridden from ToplevelWindowEventFilter:
  virtual WindowResizer* CreateWindowResizer(aura::Window* window,
                                             const gfx::Point& point,
                                             int window_component) OVERRIDE;

 private:
  friend class WorkspaceEventFilterTestHelper;

  // Updates the top-level window under the mouse so that we can change
  // the look of the caption area based on mouse-hover.
  void UpdateHoveredWindow(aura::Window* toplevel);

  // Determines if |event| corresponds to a double click on either the top or
  // bottom vertical resize edge, and if so toggles the vertical height of the
  // window between its restored state and the full available height of the
  // workspace.
  void HandleVerticalResizeDoubleClick(aura::Window* target,
                                       aura::MouseEvent* event);

  // Top-level window under the mouse cursor.
  aura::Window* hovered_window_;

  MultiWindowResizeController multi_window_resize_controller_;

  DISALLOW_COPY_AND_ASSIGN(WorkspaceEventFilter);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_WORKSPACE_WORKSPACE_EVENT_FILTER_H_
