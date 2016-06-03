// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_WM_CONTAINER_FINDER_H_
#define ASH_COMMON_WM_CONTAINER_FINDER_H_

#include "ash/ash_export.h"

namespace gfx {
class Rect;
}

namespace ash {

class WmWindow;

namespace wm {

// Returns the first ancestor of |window| that has a known type.
ASH_EXPORT WmWindow* GetContainerForWindow(WmWindow* window);

// Returns the parent to add |window| to in |context|. This is generally
// used when a window is moved from one root to another. In this case |context|
// is the new root to add |window| to.
ASH_EXPORT WmWindow* GetDefaultParent(WmWindow* context,
                                      WmWindow* window,
                                      const gfx::Rect& bounds);

}  // namespace wm
}  // namespace ash

#endif  // ASH_COMMON_WM_CONTAINER_FINDER_H_
