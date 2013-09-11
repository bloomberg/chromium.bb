// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/window_properties.h"

#include "ui/aura/window_property.h"
#include "ui/gfx/rect.h"

namespace ash {
namespace internal {
DEFINE_WINDOW_PROPERTY_KEY(bool, kAnimateToFullscreenKey, true);
DEFINE_WINDOW_PROPERTY_KEY(bool, kFullscreenUsesMinimalChromeKey, false);
DEFINE_WINDOW_PROPERTY_KEY(bool, kStayInSameRootWindowKey, false);
DEFINE_WINDOW_PROPERTY_KEY(bool, kUsesScreenCoordinatesKey, false);
DEFINE_OWNED_WINDOW_PROPERTY_KEY(gfx::Rect,
                                 kPreAutoManagedWindowBoundsKey,
                                 NULL);
DEFINE_WINDOW_PROPERTY_KEY(bool, kWindowRestoresToRestoreBounds, false);

}  // namespace internal
}  // namespace ash
