// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_WM_WM_SCREEN_UTIL_H_
#define ASH_COMMON_WM_WM_SCREEN_UTIL_H_

#include "ash/ash_export.h"

namespace gfx {
class Rect;
}

namespace ash {

class WmWindow;

namespace wm {

ASH_EXPORT gfx::Rect GetDisplayWorkAreaBoundsInParent(WmWindow* window);
ASH_EXPORT gfx::Rect GetDisplayBoundsInParent(WmWindow* window);
ASH_EXPORT gfx::Rect GetMaximizedWindowBoundsInParent(WmWindow* window);

// Returns the bounds of the physical display containing the shelf for |window|.
// Physical displays can differ from logical displays in unified desktop mode.
// TODO(oshima): Consider using physical displays in window layout, instead of
// root windows, and only use logical display in display management code.
ASH_EXPORT gfx::Rect GetDisplayBoundsWithShelf(WmWindow* window);

}  // namespace wm
}  // namespace ash

#endif  // ASH_COMMON_WM_WM_SCREEN_UTIL_H_
