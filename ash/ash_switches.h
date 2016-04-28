// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASH_SWITCHES_H_
#define ASH_ASH_SWITCHES_H_

#include "ash/ash_export.h"

#include "build/build_config.h"

namespace ash {
namespace switches {

// Note: If you add a switch, consider if it needs to be copied to a subsequent
// command line if the process executes a new copy of itself.  (For example,
// see chromeos::LoginUtil::GetOffTheRecordCommandLine().)

// Please keep alphabetized.
ASH_EXPORT extern const char kAshAnimateFromBootSplashScreen[];
ASH_EXPORT extern const char kAshCopyHostBackgroundAtBoot[];
ASH_EXPORT extern const char kAshDebugShortcuts[];
ASH_EXPORT extern const char kAshDisableMaximizeModeWindowBackdrop[];
#if defined(OS_CHROMEOS)
ASH_EXPORT extern const char kAshDisableScreenOrientationLock[];
#endif
ASH_EXPORT extern const char kAshDisableTouchExplorationMode[];
#if defined(OS_CHROMEOS)
ASH_EXPORT extern const char kAshEnableFullscreenAppList[];
ASH_EXPORT extern const char kAshEnableMagnifierKeyScroller[];
ASH_EXPORT extern const char kAshEnableTouchView[];
ASH_EXPORT extern const char kAshEnableUnifiedDesktop[];
#endif
ASH_EXPORT extern const char kAshEnableMirroredScreen[];
ASH_EXPORT extern const char kAshDisableStableOverviewOrder[];
ASH_EXPORT extern const char kAshEnableStableOverviewOrder[];
ASH_EXPORT extern const char kAshEnableSoftwareMirroring[];
ASH_EXPORT extern const char kAshDisableSystemSounds[];
ASH_EXPORT extern const char kAshEnableSystemSounds[];
ASH_EXPORT extern const char kAshEnableTouchViewTesting[];
ASH_EXPORT extern const char kAshHideNotificationsForFactory[];
ASH_EXPORT extern const char kAshHostWindowBounds[];
ASH_EXPORT extern const char kAshMaterialDesign[];
ASH_EXPORT extern const char kAshMaterialDesignDisabled[];
ASH_EXPORT extern const char kAshMaterialDesignEnabled[];
ASH_EXPORT extern const char kAshMaterialDesignExperimental[];
ASH_EXPORT extern const char kAshSecondaryDisplayLayout[];
ASH_EXPORT extern const char kAshTouchHud[];
ASH_EXPORT extern const char kAshUseFirstDisplayAsInternal[];
ASH_EXPORT extern const char kAuraLegacyPowerButton[];
#if defined(OS_WIN)
ASH_EXPORT extern const char kForceAshToDesktop[];
#endif

#if defined(OS_CHROMEOS)
// True if the pointer (cursor) position should be kept inside root windows.
ASH_EXPORT bool ConstrainPointerToRoot();
#endif

}  // namespace switches
}  // namespace ash

#endif  // ASH_ASH_SWITCHES_H_
