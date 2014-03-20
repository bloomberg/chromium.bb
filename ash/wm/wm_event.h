// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WM_EVENT_H_
#define ASH_WM_WM_EVENT_H_

#include "ash/ash_export.h"
#include "ash/wm/wm_types.h"
#include "ui/gfx/rect.h"

namespace ash {
namespace wm {

// WMEventType defines a set of operations that can change the
// window's state type and bounds.
enum WMEventType {
  // Following events are the request to become corresponding state.
  // Note that this does not mean the window will be in corresponding
  // state and the request may not be fullfilled.

  // NORMAL is used as a restore operation with a few exceptions.
  WM_EVENT_NORMAL,
  WM_EVENT_MAXIMIZE,
  WM_EVENT_MINIMIZE,
  WM_EVENT_FULLSCREEN,
  WM_EVENT_SNAP_LEFT,
  WM_EVENT_SNAP_RIGHT,

  // A window is requested to be the given bounds. The request may or
  // may not be fulfilled depending on the requested bounds and window's
  // state. This will not change the window state type.
  WM_EVENT_SET_BOUNDS,

  // Following events are compond events which may lead to different
  // states depending on the current state.

  // A user requested to toggle maximized state by double clicking window
  // header.
  WM_EVENT_TOGGLE_MAXIMIZE_CAPTION,

  // A user requested to toggle maximized state using shortcut.
  WM_EVENT_TOGGLE_MAXIMIZE,

  // A user requested to toggle vertical maximize by double clicking
  // top/bottom edge.
  WM_EVENT_TOGGLE_VERTICAL_MAXIMIZE,

  // A user requested to toggle horizontal maximize by double clicking
  // left/right edge.
  WM_EVENT_TOGGLE_HORIZONTAL_MAXIMIZE,

  // A user requested to toggle fullscreen state.
  WM_EVENT_TOGGLE_FULLSCREEN,

  // A user requested to center a window.
  WM_EVENT_CENTER,

  // TODO(oshima): Investigate if this can be removed from ash.
  // Widget requested to show in inactive state.
  WM_EVENT_SHOW_INACTIVE,

  // Following events are generated when the workspace envrionment has changed.
  // The window's state type will not be changed by these events.

  // The window is added to the workspace, either as a new window, due to
  // display disconnection or dragging.
  WM_EVENT_ADDED_TO_WORKSPACE,

  // Bounds of the display has changed.
  WM_EVENT_DISPLAY_BOUNDS_CHANGED,

  // Bounds of the work area has changed. This will not occur when the work
  // area has changed as a result of DISPLAY_BOUNDS_CHANGED.
  WM_EVENT_WORKAREA_BOUNDS_CHANGED,
};

class ASH_EXPORT WMEvent {
 public:
  explicit WMEvent(WMEventType type);
  virtual ~WMEvent();

  WMEventType type() const { return type_; }

 private:
  WMEventType type_;
  DISALLOW_COPY_AND_ASSIGN(WMEvent);
};

// An WMEvent to request new bounds for the window.
class SetBoundsEvent : public WMEvent {
public:
  SetBoundsEvent(WMEventType type, const gfx::Rect& requested_bounds);
  virtual ~SetBoundsEvent();

  const gfx::Rect& requested_bounds() const { return requested_bounds_; }

 private:
  gfx::Rect requested_bounds_;

  DISALLOW_COPY_AND_ASSIGN(SetBoundsEvent);
};

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_WM_EVENT_H_

