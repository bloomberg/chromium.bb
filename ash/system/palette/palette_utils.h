// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_PALETTE_PALETTE_UTILS_H_
#define ASH_SYSTEM_PALETTE_PALETTE_UTILS_H_

#include "ash/ash_export.h"

namespace gfx {
class Point;
}

namespace ash {
namespace palette_utils {

// Returns true if there is a stylus input device on the internal display. This
// will return false even if there is a stylus input device until hardware
// probing is complete (see ui::InputDeviceEventObserver).
ASH_EXPORT bool HasStylusInput();

// Returns true if the palette should be shown on every display.
ASH_EXPORT bool IsPaletteEnabledOnEveryDisplay();

// Returns true if the palette should be shown to the user. This happens when:
// there is a stylus input, there is an internal display, and the user has not
// disabled it in settings. This can be overridden by passing switches.
ASH_EXPORT bool ShouldShowPalette();

// Returns true if either the palette icon or the palette widget contain the
// given point (in screen space).
ASH_EXPORT bool PaletteContainsPointInScreen(const gfx::Point& point);

// Returns true if the device has an internal stylus.
ASH_EXPORT bool HasInternalStylus();

}  // namespace palette_utils
}  // namespace ash

#endif  // ASH_SYSTEM_PALETTE_PALETTE_UTILS_H_
