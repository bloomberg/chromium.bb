// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/ash_constants.h"

#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window_property.h"

namespace ash {

DEFINE_WINDOW_PROPERTY_KEY(bool, kConstrainedWindowKey, false);

const int kResizeAreaCornerSize = 16;
const int kResizeOutsideBoundsSize = 6;
const int kResizeOutsideBoundsScaleForTouch = 5;
const int kResizeInsideBoundsSize = 1;

const SkColor kChromeOsBootColor = SkColorSetRGB(0xfe, 0xfe, 0xfe);

const SkColor kFocusBorderColor = SkColorSetRGB(64, 128, 250);

}  // namespace ash
