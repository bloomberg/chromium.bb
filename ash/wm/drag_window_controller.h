// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DRAG_WINDOW_CONTROLLER_H_
#define ASH_WM_DRAG_WINDOW_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "ui/gfx/display.h"
#include "ui/gfx/rect.h"

namespace aura {
class Window;
}

namespace ui {
class Layer;
class LayerTreeOwner;
}

namespace views {
class Widget;
}

namespace ash {

// DragWindowController is responsible for showing a semi-transparent window
// while dragging a window across displays.
class ASH_EXPORT DragWindowController {
 public:
  explicit DragWindowController(aura::Window* window);
  virtual ~DragWindowController();

  // Sets the display where the window is placed after the window is dropped.
  void SetDestinationDisplay(const gfx::Display& dst_display);

  // Shows the drag window at the specified location (coordinates of the
  // parent). If |layer| is non-NULL, it is shown on top of the drag window.
  // |layer| is owned by the caller.
  // This does not immediately show the window.
  void Show();

  // Hides the drag window.
  void Hide();

  // This is used to set bounds for the drag window immediately. This should
  // be called only when the drag window is already visible.
  void SetBounds(const gfx::Rect& bounds);

  // Sets the opacity of the drag window.
  void SetOpacity(float opacity);

 private:
  FRIEND_TEST_ALL_PREFIXES(DragWindowResizerTest, DragWindowController);

  // Creates and shows the |drag_widget_| at |bounds|.
  // |layer| is shown on top of the drag window if it is non-NULL.
  // |layer| is not owned by this object.
  void CreateDragWidget(const gfx::Rect& bounds);

  // Sets bounds of the drag window. The window is shown on |dst_display_|
  // if its id() is valid. Otherwise, a display nearest to |bounds| is chosen.
  void SetBoundsInternal(const gfx::Rect& bounds);

  // Recreates a fresh layer for |window_| and all its child windows.
  void RecreateWindowLayers();

  // Window the drag window is placed beneath.
  aura::Window* window_;

  // The display where the drag is placed. When dst_display_.id() is
  // kInvalidDisplayID (i.e. the default), a display nearest to the current
  // |bounds_| is automatically used.
  gfx::Display dst_display_;

  // Initially the bounds of |window_|. Each time Show() is invoked
  // |start_bounds_| is then reset to the bounds of |drag_widget_| and
  // |bounds_| is set to the value passed into Show(). The animation animates
  // between these two values.
  gfx::Rect bounds_;

  views::Widget* drag_widget_;

  // The copy of window_->layer() and its descendants.
  scoped_ptr<ui::LayerTreeOwner> layer_owner_;

  DISALLOW_COPY_AND_ASSIGN(DragWindowController);
};

}  // namespace ash

#endif  // ASH_WM_DRAG_WINDOW_CONTROLLER_H_
