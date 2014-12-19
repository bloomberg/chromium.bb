// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DISPLAY_DISPLAY_UTIL_H_
#define ASH_DISPLAY_DISPLAY_UTIL_H_

#include <vector>

#include "ash/ash_export.h"

namespace ash {

struct DisplayMode;
class DisplayInfo;

// Creates the display mode list for internal display
// based on |native_mode|.
ASH_EXPORT std::vector<DisplayMode> CreateInternalDisplayModeList(
    const DisplayMode& native_mode);

// Returns next valid UI scale.
float GetNextUIScale(const DisplayInfo& info, bool up);

// Tests if the |info| has display mode that matches |ui_scale|.
bool HasDisplayModeForUIScale(const DisplayInfo& info, float ui_scale);

}  // namespace ash

#endif  // ASH_DISPLAY_DISPLAY_UTIL_H_
