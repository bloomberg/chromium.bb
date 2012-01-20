// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_ANIMATIONS_H_
#define ASH_WM_WINDOW_ANIMATIONS_H_
#pragma once

#include "ash/ash_export.h"

namespace aura {
class Window;
}

namespace ash {

// A variety of canned animations for window transitions.
enum WindowVisibilityAnimationType {
  WINDOW_VISIBILITY_ANIMATION_TYPE_DROP = 0,  // Default. Window shrinks in.
  WINDOW_VISIBILITY_ANIMATION_TYPE_VERTICAL   // Vertical Glenimation.
};

void ASH_EXPORT SetWindowVisibilityAnimationType(
    aura::Window* window,
    WindowVisibilityAnimationType type);

namespace internal {

void AnimateShowWindow(aura::Window* window);
void AnimateHideWindow(aura::Window* window);

}  // namespace internal
}  // namespace ash


#endif  // ASH_WM_WINDOW_ANIMATIONS_H_
