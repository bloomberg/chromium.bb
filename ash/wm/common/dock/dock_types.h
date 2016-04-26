// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_COMMON_DOCK_DOCK_TYPES_H_
#define ASH_WM_COMMON_DOCK_DOCK_TYPES_H_

namespace ash {

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

// User action recorded for use in UMA histograms.
enum DockedAction {
  DOCKED_ACTION_NONE,      // Regular drag of undocked window. Not recorded.
  DOCKED_ACTION_DOCK,      // Dragged and docked a window.
  DOCKED_ACTION_UNDOCK,    // Dragged and undocked a window.
  DOCKED_ACTION_RESIZE,    // Resized a docked window.
  DOCKED_ACTION_REORDER,   // Possibly reordered docked windows.
  DOCKED_ACTION_EVICT,     // A docked window could not stay docked.
  DOCKED_ACTION_MAXIMIZE,  // Maximized a docked window.
  DOCKED_ACTION_MINIMIZE,  // Minimized a docked window.
  DOCKED_ACTION_RESTORE,   // Restored a docked window that was minimized.
  DOCKED_ACTION_CLOSE,     // Closed a window while it was docked.
  DOCKED_ACTION_COUNT,     // Maximum value of this enum for histograms use.
};

// Event source for the docking user action (when known).
enum DockedActionSource {
  DOCKED_ACTION_SOURCE_UNKNOWN,
  DOCKED_ACTION_SOURCE_MOUSE,
  DOCKED_ACTION_SOURCE_TOUCH,
  DOCKED_ACTION_SOURCE_KEYBOARD,

  // Maximum value of this enum for histograms use.
  DOCKED_ACTION_SOURCE_COUNT,
};

}  // namespace ash

#endif  // ASH_WM_COMMON_DOCK_DOCK_TYPES_H_
