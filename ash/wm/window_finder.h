// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_WINDOW_FINDER_H_
#define ASH_WM_WINDOW_FINDER_H_

#include <set>

#include "ash/ash_export.h"

namespace aura {
class Window;
}

namespace gfx {
class Point;
}

namespace ash {
namespace wm {

// Finds the topmost window at |screen_point| with ignoring |ignore|. If
// |real_topmost| is not nullptr, it will be updated to the topmost visible
// window regardless of |ignore|. If overview is active when this function is
// called, the overview window that contains |screen_point| will be returned.
// Note this overview window might not be visibile (e.g., it represents an aura
// window whose window state is MINIMIZED).
ASH_EXPORT aura::Window* GetTopmostWindowAtPoint(
    const gfx::Point& screen_point,
    const std::set<aura::Window*>& ignore,
    aura::Window** real_topmost);

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_WINDOW_FINDER_H_
