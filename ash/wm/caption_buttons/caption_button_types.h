// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_CAPTION_BUTTONS_CAPTION_BUTTON_TYPES_H_
#define ASH_WM_CAPTION_BUTTONS_CAPTION_BUTTON_TYPES_H_

namespace ash {

// These are the types of maximization we know.
enum MaximizeBubbleFrameState {
  FRAME_STATE_NONE = 0,
  FRAME_STATE_FULL = 1,  // This is the full maximized state.
  FRAME_STATE_SNAP_LEFT = 2,
  FRAME_STATE_SNAP_RIGHT = 3
};

// These are the icon types that a caption button can have. The size button's
// action (SnapType) can be different from its icon.
enum CaptionButtonIcon {
  CAPTION_BUTTON_ICON_MINIMIZE,
  CAPTION_BUTTON_ICON_MAXIMIZE_RESTORE,
  CAPTION_BUTTON_ICON_CLOSE,
  CAPTION_BUTTON_ICON_LEFT_SNAPPED,
  CAPTION_BUTTON_ICON_RIGHT_SNAPPED,
};

} // namespace ash

#endif  // ASH_WM_CAPTION_BUTTONS_CAPTION_BUTTON_TYPES_H_
