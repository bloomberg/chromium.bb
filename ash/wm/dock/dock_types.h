// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DOCK_DOCK_TYPES_H_
#define ASH_WM_DOCK_DOCK_TYPES_H_

namespace ash {

namespace internal {

// Possible values of which side of the screen the windows are docked at.
// This is used by DockedwindowLayoutManager and DockedWindowResizer to
// implement docking behavior including magnetism while dragging windows into
// or out of the docked windows area.
enum DockedAlignment {
  // No docked windows.
  DOCKED_ALIGNMENT_NONE,

  // Some windows are already docked on the left side of the screen.
  DOCKED_ALIGNMENT_LEFT,

  // Some windows are already docked on the right side of the screen.
  DOCKED_ALIGNMENT_RIGHT,
};

}  // namespace internal
}  // namespace ash

#endif  // ASH_WM_DOCK_DOCK_TYPES_H_
