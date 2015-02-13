// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_OVERVIEW_OVERVIEW_ANIMATION_TYPE_H_
#define ASH_WM_OVERVIEW_OVERVIEW_ANIMATION_TYPE_H_

namespace ash {

// Enumeration of the different overview mode animations.
enum OverviewAnimationType {
  // TODO(bruthig): Remove OVERVIEW_ANIMATION_NONE value and replace it with
  // correct animation type actions.
  OVERVIEW_ANIMATION_NONE,
  // Used to fade in the close button and label.
  OVERVIEW_ANIMATION_ENTER_OVERVIEW_MODE_FADE_IN,
  // Used to fade out the close button.
  OVERVIEW_ANIMATION_ENTER_OVERVIEW_MODE_FADE_OUT,
  // Used to position windows when entering/exiting overview mode and when a
  // window is closed while overview mode is active.
  OVERVIEW_ANIMATION_LAY_OUT_SELECTOR_ITEMS,
  // Used to hide non-overview mode windows.
  OVERVIEW_ANIMATION_HIDE_WINDOW,
  // Used to restore windows to their original position when exiting overview
  // mode.
  OVERVIEW_ANIMATION_RESTORE_WINDOW,
  // Used to animate windows when a user performs a touch drag gesture.
  OVERVIEW_ANIMATION_SCROLL_SELECTOR_ITEM,
  // Used to animate windows back in to position when a touch drag gesture is
  // cancelled.
  OVERVIEW_ANIMATION_CANCEL_SELECTOR_ITEM_SCROLL
};

}  // namespace ash

#endif  // ASH_WM_OVERVIEW_OVERVIEW_ANIMATION_TYPE_H_
