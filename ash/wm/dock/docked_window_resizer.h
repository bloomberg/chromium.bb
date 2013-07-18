// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DOCK_DOCK_WINDOW_RESIZER_H_
#define ASH_WM_DOCK_DOCK_WINDOW_RESIZER_H_

#include "ash/wm/window_resizer.h"
#include "base/compiler_specific.h"

namespace gfx {
class Point;
class Rect;
}

namespace aura {
class RootWindow;
}

namespace ash {

namespace internal {
class DockedWindowLayoutManager;
}

// DockWindowResizer is used by ToplevelWindowEventFilter to handle dragging,
// moving or resizing of a window while it is docked to the side of a screen.
class ASH_EXPORT DockedWindowResizer : public WindowResizer {
 public:
  virtual ~DockedWindowResizer();

  // Creates a new DockWindowResizer. The caller takes ownership of the
  // returned object. The ownership of |next_window_resizer| is taken by the
  // returned object. Returns NULL if not resizable.
  static DockedWindowResizer* Create(WindowResizer* next_window_resizer,
                                     aura::Window* window,
                                     const gfx::Point& location,
                                     int window_component,
                                     aura::client::WindowMoveSource source);

  // WindowResizer:
  virtual void Drag(const gfx::Point& location, int event_flags) OVERRIDE;
  virtual void CompleteDrag(int event_flags) OVERRIDE;
  virtual void RevertDrag() OVERRIDE;
  virtual aura::Window* GetTarget() OVERRIDE;
  virtual const gfx::Point& GetInitialLocation() const OVERRIDE;

 private:
  // Creates DockWindowResizer that adds the ability to attach / detach
  // windows to / from the dock. This object takes ownership of
  // |next_window_resizer|.
  DockedWindowResizer(WindowResizer* next_window_resizer,
                      const Details& details);

  // Checks if the provided window bounds should snap to the side of a screen.
  // If so the offset returned gives the necessary adjustment to snap.
  bool MaybeSnapToEdge(const gfx::Rect& bounds, gfx::Point* offset);

  // Tracks the window's initial position and attachment at the start of a drag
  // and informs the DockLayoutManager that a drag has started if necessary.
  void StartedDragging();

  // Informs the DockLayoutManager that the drag is complete if it was informed
  // of the drag start.
  void FinishDragging();

  const Details details_;

  gfx::Point last_location_;

  // Wraps a window resizer and adds detaching / reattaching during drags.
  scoped_ptr<WindowResizer> next_window_resizer_;

  // Dock container window.
  internal::DockedWindowLayoutManager* dock_layout_;

  // Set to true once Drag() is invoked and the bounds of the window change.
  bool did_move_or_resize_;

  // Set to true if the window that is being dragged was docked before drag.
  bool was_docked_;

  // If non-NULL the destructor sets this to true. Used to determine if this has
  // been deleted.
  bool* destroyed_;

  DISALLOW_COPY_AND_ASSIGN(DockedWindowResizer);
};

}  // namespace ash

#endif  // ASH_WM_DOCK_DOCK_WINDOW_RESIZER_H_
