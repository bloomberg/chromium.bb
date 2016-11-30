// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/common/ash_constants.h"

#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/color_palette.h"

namespace ash {

const int kResizeAreaCornerSize = 16;
const int kResizeOutsideBoundsSize = 6;
const int kResizeOutsideBoundsScaleForTouch = 5;
const int kResizeInsideBoundsSize = 1;

#if defined(OS_CHROMEOS)
const SkColor kChromeOsBootColor = SkColorSetRGB(0xfe, 0xfe, 0xfe);
#endif

const SkColor kFocusBorderColor = SkColorSetA(gfx::kGoogleBlue500, 0x99);
const float kFocusBorderThickness = 2.f;

}  // namespace ash
