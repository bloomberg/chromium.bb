// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WORKSPACE_MAXIMIZE_BUBBLE_FRAME_STATE_H_
#define ASH_WM_WORKSPACE_MAXIMIZE_BUBBLE_FRAME_STATE_H_

namespace ash {

// These are the types of maximization we know.
enum MaximizeBubbleFrameState {
  FRAME_STATE_NONE = 0,
  FRAME_STATE_FULL = 1,  // This is the full maximized state.
  FRAME_STATE_SNAP_LEFT = 2,
  FRAME_STATE_SNAP_RIGHT = 3
};

} // namespace views

#endif  // ASH_WM_WORKSPACE_MAXIMIZE_BUBBLE_FRAME_STATE_H_
