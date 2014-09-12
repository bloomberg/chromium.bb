// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TOUCH_TOUCHSCREEN_UTIL_H_
#define ASH_TOUCH_TOUCHSCREEN_UTIL_H_

#include <vector>

#include "ash/ash_export.h"
#include "ash/display/display_info.h"
#include "ui/events/touchscreen_device.h"

namespace ash {

// Given a list of displays and a list of touchscreens, associate them. The
// information in |displays| will be updated to reflect the mapping.
ASH_EXPORT void AssociateTouchscreens(
    std::vector<DisplayInfo>* displays,
    const std::vector<ui::TouchscreenDevice>& touchscreens);

}  // namespace ash

#endif  // ASH_TOUCH_TOUCHSCREEN_UTIL_H_
