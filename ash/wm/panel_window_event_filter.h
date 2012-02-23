// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_PANEL_WINDOW_EVENT_FILTER_H
#define ASH_WM_PANEL_WINDOW_EVENT_FILTER_H
#pragma once

#include "ui/aura/event_filter.h"
#include "ash/wm/panel_layout_manager.h"
#include "ui/gfx/point.h"
#include "ui/gfx/rect.h"

namespace aura {
class LocatedEvent;
class MouseEvent;
class Window;
}

namespace ash {
namespace internal {

class PanelWindowEventFilter : public aura::EventFilter {
 public:
  PanelWindowEventFilter(aura::Window* panel_container,
                         PanelLayoutManager* layout_manager);
  virtual ~PanelWindowEventFilter();

  // Overriden from aura::EventFilter:
  virtual bool PreHandleKeyEvent(aura::Window* target,
                                 aura::KeyEvent* event) OVERRIDE;
  virtual bool PreHandleMouseEvent(aura::Window* target,
                                   aura::MouseEvent* event) OVERRIDE;
  virtual ui::TouchStatus PreHandleTouchEvent(aura::Window* target,
                                              aura::TouchEvent* event) OVERRIDE;
  virtual ui::GestureStatus PreHandleGestureEvent(
      aura::Window* target,
      aura::GestureEvent* event) OVERRIDE;

 private:
  enum DragState {
    DRAG_NONE,
    DRAG_CLICKED,
    DRAG_STARTED
  };

  bool HandleDrag(aura::Window* target, aura::LocatedEvent* event);
  void FinishDrag();

  aura::Window* panel_container_;
  PanelLayoutManager* layout_manager_;
  gfx::Point drag_origin_;
  gfx::Point drag_location_in_dragged_window_;
  aura::Window* dragged_panel_;
  DragState drag_state_;

  DISALLOW_COPY_AND_ASSIGN(PanelWindowEventFilter);
};

}
}
#endif  // ASH_WM_PANEL_EVENT_FILTER_H
