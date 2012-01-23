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
  WINDOW_VISIBILITY_ANIMATION_TYPE_DEFAULT = 0, // Default. Lets the system
                                                // decide based on window type.
  WINDOW_VISIBILITY_ANIMATION_TYPE_DROP,        // Window shrinks in.
  WINDOW_VISIBILITY_ANIMATION_TYPE_VERTICAL,    // Vertical Glenimation.
  WINDOW_VISIBILITY_ANIMATION_TYPE_FADE         // Fades in/out.
};

void ASH_EXPORT SetWindowVisibilityAnimationType(
    aura::Window* window,
    WindowVisibilityAnimationType type);

namespace internal {

void AnimateOnChildWindowVisibilityChanged(aura::Window* window, bool visible);

}  // namespace internal
}  // namespace ash


#endif  // ASH_WM_WINDOW_ANIMATIONS_H_
