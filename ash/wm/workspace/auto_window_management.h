// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_export.h"

namespace gfx {
class Rect;
}
namespace aura {
class RootWindow;
class Window;
}

namespace ash {
namespace internal {

// Check if after removal or hide of the given |removed_window| an automated
// desktop location management can be performed and rearrange accordingly.
void RearrangeVisibleWindowOnHideOrRemove(const aura::Window* removed_window);

// Check if after insertion or showing of the given |added_window| an automated
// desktop location management can be performed and rearrange accordingly.
void RearrangeVisibleWindowOnShow(aura::Window* added_window);

} // namespace internal

// Returns the top window used for automated windows location
// management.
// TODO(oshima): This is temporarily made public and exported until we
// move the initial bounds logic in window_sizer_ash.cc into ash.
ASH_EXPORT aura::Window* GetTopWindowForNewWindow(
    const aura::RootWindow* root_window);

} // namespace ash
