// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DRAG_WINDOW_RESIZER_H_
#define ASH_WM_DRAG_WINDOW_RESIZER_H_

#include "ash/wm/window_resizer.h"
#include "base/compiler_specific.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "ui/gfx/point.h"

namespace ash {

class DragWindowController;

// DragWindowResizer is a decorator of WindowResizer and adds the ability to
// drag windows across displays.
class ASH_EXPORT DragWindowResizer : public WindowResizer {
 public:
  virtual ~DragWindowResizer();

  // Creates a new DragWindowResizer. The caller takes ownership of the
  // returned object. The ownership of |next_window_resizer| is taken by the
  // returned object. Returns NULL if not resizable.
  static DragWindowResizer* Create(WindowResizer* next_window_resizer,
                                   wm::WindowState* window_state);

  // WindowResizer:
  virtual void Drag(const gfx::Point& location, int event_flags) OVERRIDE;
  virtual void CompleteDrag() OVERRIDE;
  virtual void RevertDrag() OVERRIDE;

 private:
  FRIEND_TEST_ALL_PREFIXES(DragWindowResizerTest, DragWindowController);

  // Creates DragWindowResizer that adds the ability of dragging windows across
  // displays to |next_window_resizer|. This object takes the ownership of
  // |next_window_resizer|.
  explicit DragWindowResizer(WindowResizer* next_window_resizer,
                             wm::WindowState* window_state);

  // Updates the bounds of the phantom window for window dragging. Set true on
  // |in_original_root| if the pointer is still in |window()->GetRootWindow()|.
  void UpdateDragWindow(const gfx::Rect& bounds, bool in_original_root);

  // Returns true if we should allow the mouse pointer to warp.
  bool ShouldAllowMouseWarp();

  scoped_ptr<WindowResizer> next_window_resizer_;

  // Shows a semi-transparent image of the window being dragged.
  scoped_ptr<DragWindowController> drag_window_controller_;

  gfx::Point last_mouse_location_;

  // Current instance for use by the DragWindowResizerTest.
  static DragWindowResizer* instance_;

  base::WeakPtrFactory<DragWindowResizer> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(DragWindowResizer);
};

}  // namespace ash

#endif  // ASH_WM_DRAG_WINDOW_RESIZER_H_
