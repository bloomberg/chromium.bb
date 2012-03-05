// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DEFAULT_WINDOW_RESIZER_H_
#define ASH_WM_DEFAULT_WINDOW_RESIZER_H_
#pragma once

#include "ash/wm/window_resizer.h"
#include "base/compiler_specific.h"

namespace ash {

namespace internal {
class RootWindowEventFilter;
}

// WindowResizer is used by ToplevelWindowEventFilter to handle dragging, moving
// or resizing a window. All coordinates passed to this are in the parent
// windows coordiantes.
class ASH_EXPORT DefaultWindowResizer : public WindowResizer {
 public:
  // Constants to identify the type of resize.
  static const int kBoundsChange_None;
  static const int kBoundsChange_Repositions;
  static const int kBoundsChange_Resizes;

  // Used to indicate which direction the resize occurs in.
  static const int kBoundsChangeDirection_None;
  static const int kBoundsChangeDirection_Horizontal;
  static const int kBoundsChangeDirection_Vertical;

  virtual ~DefaultWindowResizer();

  // Creates a new DefaultWindowResizer. The caller takes ownership of the
  // returned object. Returns NULL if not resizable.
  static DefaultWindowResizer* Create(aura::Window* window,
                                      const gfx::Point& location,
                                      int window_component,
                                      int grid_size);

  // Returns true if the drag will result in changing the window in anyway.
  bool is_resizable() const { return details_.is_resizable; }

  // WindowResizer overides:
  virtual void Drag(const gfx::Point& location) OVERRIDE;
  virtual void CompleteDrag() OVERRIDE;
  virtual void RevertDrag() OVERRIDE;

 private:
  explicit DefaultWindowResizer(const Details& details);

  const Details details_;

  // Set to true once Drag() is invoked and the bounds of the window change.
  bool did_move_or_resize_;

  internal::RootWindowEventFilter* root_filter_;

  DISALLOW_COPY_AND_ASSIGN(DefaultWindowResizer);
};

}  // namespace aura

#endif  // ASH_WM_DEFAULT_WINDOW_RESIZER_H_
