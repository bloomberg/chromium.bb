// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_ROOT_WINDOW_EVENT_FILTER_H_
#define ASH_WM_ROOT_WINDOW_EVENT_FILTER_H_
#pragma once

#include "base/compiler_specific.h"
#include "base/observer_list.h"
#include "ui/aura/event.h"
#include "ui/aura/event_filter.h"
#include "ash/ash_export.h"

namespace ash {
namespace internal {

// RootWindowEventFilter gets all root window events first and can provide
// actions to those events. It implements root window features such as click to
// activate a window and cursor change when moving mouse.
// Additional event filters can be added to RootWIndowEventFilter. Events will
// pass through those additional filters in their addition order and could be
// consumed by any of those filters. If an event is consumed by a filter, the
// rest of the filter(s) and RootWindowEventFilter will not see the consumed
// event.
class ASH_EXPORT RootWindowEventFilter : public aura::EventFilter {
 public:
  RootWindowEventFilter();
  virtual ~RootWindowEventFilter();

  // Adds/removes additional event filters.
  void AddFilter(aura::EventFilter* filter);
  void RemoveFilter(aura::EventFilter* filter);
  size_t GetFilterCount() const;

  // Overridden from EventFilter:
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
  // Updates the cursor if the target provides a custom one, and provides
  // default resize cursors for window edges.
  void UpdateCursor(aura::Window* target, aura::MouseEvent* event);

  // Sets the cursor invisible when the target receives touch press event.
  void SetCursorVisible(aura::Window* target,
                        aura::LocatedEvent* event,
                        bool show);

  // Dispatches event to additional filters. Returns false or
  // ui::TOUCH_STATUS_UNKNOWN if event is consumed.
  bool FilterKeyEvent(aura::Window* target, aura::KeyEvent* event);
  bool FilterMouseEvent(aura::Window* target, aura::MouseEvent* event);
  ui::TouchStatus FilterTouchEvent(aura::Window* target,
                                   aura::TouchEvent* event);

  // Additional event filters that pre-handles events.
  ObserverList<aura::EventFilter, true> filters_;

  DISALLOW_COPY_AND_ASSIGN(RootWindowEventFilter);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_ROOT_WINDOW_EVENT_FILTER_H_
