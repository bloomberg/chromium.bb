// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_MULTI_WINDOW_RESIZE_CONTROLLER_H_
#define ASH_WM_WORKSPACE_MULTI_WINDOW_RESIZE_CONTROLLER_H_
#pragma once

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/timer.h"
#include "ui/gfx/rect.h"
#include "ui/views/mouse_watcher.h"

namespace aura {
class Window;
}

namespace views {
class Widget;
}

namespace ash {
namespace internal {

class WorkspaceWindowResizer;

// Two directions resizes happen in.
enum Direction {
  TOP_BOTTOM,
  LEFT_RIGHT,
};

// MultiWindowResizeController is responsible for determining and showing a
// widget that allows resizing multiple windows at the same time.
// MultiWindowResizeController is driven by WorkspaceEventFilter.
class ASH_EXPORT MultiWindowResizeController :
    public views::MouseWatcherListener {
 public:
  MultiWindowResizeController();
  virtual ~MultiWindowResizeController();

  // If necessary, shows the resize widget. |window| is the window the mouse
  // is over, |component| the edge and |point| the location of the mouse.
  void Show(aura::Window* window, int component, const gfx::Point& point);

  // Hides the resize widget.
  void Hide();

  void set_grid_size(int grid_size) { grid_size_ = grid_size; }

  // MouseWatcherListenre overrides:
  virtual void MouseMovedOutOfHost() OVERRIDE;

 private:
  // Used to track the two resizable windows and direction.
  struct ResizeWindows {
    ResizeWindows();
    ~ResizeWindows();

    // Returns true if |other| equals this ResizeWindows.
    bool Equals(const ResizeWindows& other) const;

    // Returns true if this ResizeWindows is valid.
    bool is_valid() const { return window1 && window2; }

    // The left/top window to resize.
    aura::Window* window1;

    // Other window to resize.
    aura::Window* window2;

    // Direction
    Direction direction;
  };

  class ResizeMouseWatcherHost;
  class ResizeView;

  // Returns a ResizeWindows based on the specified arguments. Use is_valid()
  // to test if the return value is a valid multi window resize location.
  ResizeWindows DetermineWindows(aura::Window* window,
                                 int window_component,
                                 const gfx::Point& point) const;

  // Finds a window by edge (one of the constants HitTestCompat.
  aura::Window* FindWindowByEdge(aura::Window* window_to_ignore,
                                 int edge_want,
                                 int x,
                                 int y) const;

  // Shows the widget immediately.
  void ShowNow();

  // Returns true if the widget is showing.
  bool IsShowing() const;

  // Initiates a resize.
  void StartResize(const gfx::Point& screen_location);

  // Resizes to the new location.
  void Resize(const gfx::Point& screen_location);

  // Completes the resize.
  void CompleteResize();

  // Cancels the resize.
  void CancelResize();

  // Returns the bounds for the resize widget.
  gfx::Rect CalculateResizeWidgetBounds() const;

  // Returns true if |screen_location| is over the resize windows (or the resize
  // widget itself).
  bool IsOverWindows(const gfx::Point& screen_location) const;

  // Returns true if |screen_location| is over |window|.
  bool IsOverWindow(aura::Window* window,
                    const gfx::Point& screen_location) const;

  // Windows and direction to resize.
  ResizeWindows windows_;

  // Timer used before showing.
  base::OneShotTimer<MultiWindowResizeController> show_timer_;

  views::Widget* resize_widget_;

  // If non-null we're in a resize loop.
  scoped_ptr<WorkspaceWindowResizer> window_resizer_;

  // Bounds the widget was last shown at.
  gfx::Rect show_bounds_;

  // Size of the grid.
  int grid_size_;

  DISALLOW_COPY_AND_ASSIGN(MultiWindowResizeController);
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_WORKSPACE_MULTI_WINDOW_RESIZE_CONTROLLER_H_
