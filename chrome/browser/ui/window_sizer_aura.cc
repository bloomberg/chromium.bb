// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/window_sizer.h"

// This doesn't matter for aura, which has different tiling.
// static
const int WindowSizer::kWindowTilePixels = 10;

// static
gfx::Point WindowSizer::GetDefaultPopupOrigin(const gfx::Size& size) {
  // TODO(oshima):This is used to control panel/popups, and this may not be
  // needed on aura environment as they must be controlled by WM.
  return gfx::Point();
}
