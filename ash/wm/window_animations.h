// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_ANIMATIONS_H_
#define ASH_WM_WINDOW_ANIMATIONS_H_
#pragma once

namespace aura {
class Window;
}

namespace ash {
namespace internal {

// Implements a variety of canned animations for window transitions.

void AnimateShowWindow(aura::Window* window);
void AnimateHideWindow(aura::Window* window);

}  // namespace internal
}  // namespace ash


#endif  // ASH_WM_WINDOW_ANIMATIONS_H_
