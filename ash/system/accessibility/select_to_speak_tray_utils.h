// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_ACCESSIBILITY_SELECT_TO_SPEAK_TRAY_UTILS_H_
#define ASH_SYSTEM_ACCESSIBILITY_SELECT_TO_SPEAK_TRAY_UTILS_H_

#include "ash/ash_export.h"

namespace gfx {
class Point;
}

namespace ash {
namespace select_to_speak_tray_utils {

// Returns true if either the select-to-speak tray icon contains the
// given point (in screen space).
ASH_EXPORT bool SelectToSpeakTrayContainsPointInScreen(const gfx::Point& point);

}  // namespace select_to_speak_tray_utils
}  // namespace ash

#endif  // ASH_SYSTEM_ACCESSIBILITY_SELECT_TO_SPEAK_TRAY_UTILS_H_
