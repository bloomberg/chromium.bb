// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_SNAP_TYPES_H_
#define ASH_WM_WORKSPACE_SNAP_TYPES_H_

namespace ash {

// These are the window snap types which can be used for window resizing.
// Their main use case is the class FrameMaximizeButton.
enum SnapType {
  SNAP_LEFT,
  SNAP_RIGHT,
  SNAP_MAXIMIZE,
  SNAP_MINIMIZE,
  SNAP_RESTORE,
  SNAP_NONE
};

}  // namespace ash

#endif  // ASH_WM_WORKSPACE_SNAP_TYPES_H_
