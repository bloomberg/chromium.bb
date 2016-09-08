// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_SYSTEM_CHROMEOS_PALETTE_PALETTE_UTILS_H_
#define ASH_COMMON_SYSTEM_CHROMEOS_PALETTE_PALETTE_UTILS_H_

#include "ash/ash_export.h"

namespace gfx {
class Point;
}

namespace ash {

// Returns true if the palette feature is enabled. The palette itself may have
// been disabled by the user.
ASH_EXPORT bool IsPaletteFeatureEnabled();

// Are experimental palette features enabled?
ASH_EXPORT bool ArePaletteExperimentalFeaturesEnabled();

// Returns true if the palette should be shown on every display.
ASH_EXPORT bool IsPaletteEnabledOnEveryDisplay();

// Returns true if either the palette icon or the palette widget contain the
// given point (in screen space).
ASH_EXPORT bool PaletteContainsPointInScreen(const gfx::Point& point);

}  // namespace ash

#endif  // ASH_COMMON_SYSTEM_CHROMEOS_PALETTE_PALETTE_UTILS_H_
