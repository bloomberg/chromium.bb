// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_COMMON_ROOT_WINDOW_FINDER_H_
#define ASH_WM_COMMON_ROOT_WINDOW_FINDER_H_

#include "ash/wm/common/ash_wm_common_export.h"

namespace gfx {
class Point;
class Rect;
}  // namespace gfx

namespace ash {
namespace wm {

class WmWindow;

// Returns the RootWindow at |point| in the virtual screen coordinates.
// Returns NULL if the root window does not exist at the given
// point.
ASH_WM_COMMON_EXPORT WmWindow* GetRootWindowAt(const gfx::Point& point);

// Returns the RootWindow that shares the most area with |rect| in
// the virtual scren coordinates.
ASH_WM_COMMON_EXPORT WmWindow* GetRootWindowMatching(const gfx::Rect& rect);

}  // namespace wm
}  // namespace ash

#endif  // ASH_WM_COMMON_ROOT_WINDOW_FINDER_H_
