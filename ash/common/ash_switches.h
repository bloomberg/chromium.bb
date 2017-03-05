// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMMON_ASH_SWITCHES_H_
#define ASH_COMMON_ASH_SWITCHES_H_

#include "ash/ash_export.h"

namespace ash {
namespace switches {

// Note: If you add a switch, consider if it needs to be copied to a subsequent
// command line if the process executes a new copy of itself.  (For example,
// see chromeos::LoginUtil::GetOffTheRecordCommandLine().)

// Please keep alphabetized.
// TODO(sky): fix order!
ASH_EXPORT extern const char kAshAdjustableLargeCursor[];
ASH_EXPORT extern const char kAshAnimateFromBootSplashScreen[];
ASH_EXPORT extern const char kAshCopyHostBackgroundAtBoot[];
ASH_EXPORT extern const char kAshDebugShortcuts[];
ASH_EXPORT extern const char kAshDeveloperShortcuts[];
ASH_EXPORT extern const char kAshDisableMaximizeModeWindowBackdrop[];
ASH_EXPORT extern const char kAshDisableScreenOrientationLock[];
ASH_EXPORT extern const char kAshDisableTouchExplorationMode[];
ASH_EXPORT extern const char kAshEnableMagnifierKeyScroller[];
ASH_EXPORT extern const char kAshEnablePaletteOnAllDisplays[];
ASH_EXPORT extern const char kAshEnableDockedWindows[];
ASH_EXPORT extern const char kAshEnableTouchView[];
ASH_EXPORT extern const char kAshEnableMirroredScreen[];
ASH_EXPORT extern const char kAshForceEnablePalette[];
ASH_EXPORT extern const char kAshHideNotificationsForFactory[];
ASH_EXPORT extern const char kAshShelfColor[];
ASH_EXPORT extern const char kAshShelfColorLightMuted[];
ASH_EXPORT extern const char kAshShelfColorLightVibrant[];
ASH_EXPORT extern const char kAshShelfColorNormalMuted[];
ASH_EXPORT extern const char kAshShelfColorNormalVibrant[];
ASH_EXPORT extern const char kAshShelfColorDarkMuted[];
ASH_EXPORT extern const char kAshShelfColorDarkVibrant[];
ASH_EXPORT extern const char kAshTouchHud[];
ASH_EXPORT extern const char kAuraLegacyPowerButton[];

// True if the pointer (cursor) position should be kept inside root windows.
ASH_EXPORT bool ConstrainPointerToRoot();

// True if docking windows right or left is enabled.
ASH_EXPORT bool DockedWindowsEnabled();

}  // namespace switches
}  // namespace ash

#endif  // ASH_COMMON_ASH_SWITCHES_H_
