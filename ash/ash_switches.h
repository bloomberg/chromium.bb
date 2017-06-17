// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASH_SWITCHES_H_
#define ASH_ASH_SWITCHES_H_

#include "ash/ash_export.h"

namespace ash {
namespace switches {

// Note: If you add a switch, consider if it needs to be copied to a subsequent
// command line if the process executes a new copy of itself.  (For example,
// see chromeos::LoginUtil::GetOffTheRecordCommandLine().)

// Please keep alphabetized.
// TODO(sky): fix order!
ASH_EXPORT extern const char kAshAnimateFromBootSplashScreen[];
ASH_EXPORT extern const char kAshCopyHostBackgroundAtBoot[];
ASH_EXPORT extern const char kAshDebugShortcuts[];
ASH_EXPORT extern const char kAshDeveloperShortcuts[];
ASH_EXPORT extern const char kAshDisableSmoothScreenRotation[];
ASH_EXPORT extern const char kAshDisableTouchExplorationMode[];
ASH_EXPORT extern const char kAshEnableMagnifierKeyScroller[];
ASH_EXPORT extern const char kAshEnableNightLight[];
ASH_EXPORT extern const char kAshEnablePaletteOnAllDisplays[];
ASH_EXPORT extern const char kAshEnableScaleSettingsTray[];
ASH_EXPORT extern const char kAshEnableTouchView[];
ASH_EXPORT extern const char kAshEnableMirroredScreen[];
ASH_EXPORT extern const char kAshEstimatedPresentationDelay[];
ASH_EXPORT extern const char kAshForceEnableStylusTools[];
ASH_EXPORT extern const char kAshForceTabletMode[];
ASH_EXPORT extern const char kAshForceTabletModeAuto[];
ASH_EXPORT extern const char kAshForceTabletModeClamshell[];
ASH_EXPORT extern const char kAshForceTabletModeTouchView[];
ASH_EXPORT extern const char kAshHideNotificationsForFactory[];
ASH_EXPORT extern const char kAshShelfColor[];
ASH_EXPORT extern const char kAshShelfColorEnabled[];
ASH_EXPORT extern const char kAshShelfColorDisabled[];
ASH_EXPORT extern const char kAshShelfColorScheme[];
ASH_EXPORT extern const char kAshShelfColorSchemeLightMuted[];
ASH_EXPORT extern const char kAshShelfColorSchemeLightVibrant[];
ASH_EXPORT extern const char kAshShelfColorSchemeNormalMuted[];
ASH_EXPORT extern const char kAshShelfColorSchemeNormalVibrant[];
ASH_EXPORT extern const char kAshShelfColorSchemeDarkMuted[];
ASH_EXPORT extern const char kAshShelfColorSchemeDarkVibrant[];
ASH_EXPORT extern const char kAshTouchHud[];
ASH_EXPORT extern const char kAuraLegacyPowerButton[];
ASH_EXPORT extern const char kUseIMEService[];

// True if the pointer (cursor) position should be kept inside root windows.
ASH_EXPORT bool ConstrainPointerToRoot();

}  // namespace switches
}  // namespace ash

#endif  // ASH_ASH_SWITCHES_H_
