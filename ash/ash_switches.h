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
ASH_EXPORT extern const char kAshConstrainPointerToRoot[];
ASH_EXPORT extern const char kAshCopyHostBackgroundAtBoot[];
ASH_EXPORT extern const char kAshDebugShortcuts[];
ASH_EXPORT extern const char kAshDefaultWallpaperIsOem[];
ASH_EXPORT extern const char kAshDefaultWallpaperLarge[];
ASH_EXPORT extern const char kAshDefaultWallpaperSmall[];
ASH_EXPORT extern const char kAshDisableLockLayoutManager[];
ASH_EXPORT extern const char kAshDisableTouchExplorationMode[];
#if defined(OS_CHROMEOS)
ASH_EXPORT extern const char kAshEnableMagnifierKeyScroller[];
#endif
ASH_EXPORT extern const char kAshDisableTextFilteringInOverviewMode[];
ASH_EXPORT extern const char kAshEnablePowerButtonQuickLock[];
ASH_EXPORT extern const char kAshEnableSoftwareMirroring[];
ASH_EXPORT extern const char kAshEnableSystemSounds[];
ASH_EXPORT extern const char kAshEnableTouchViewTesting[];
ASH_EXPORT extern const char kAshEnableTouchViewTouchFeedback[];
ASH_EXPORT extern const char kAshEnableTrayDragging[];
ASH_EXPORT extern const char kAshGuestWallpaperLarge[];
ASH_EXPORT extern const char kAshGuestWallpaperSmall[];
ASH_EXPORT extern const char kAshHideNotificationsForFactory[];
ASH_EXPORT extern const char kAshHostWindowBounds[];
ASH_EXPORT extern const char kAshSecondaryDisplayLayout[];
ASH_EXPORT extern const char kAshTouchHud[];
ASH_EXPORT extern const char kAshUseFirstDisplayAsInternal[];
ASH_EXPORT extern const char kAuraLegacyPowerButton[];
#if defined(OS_WIN)
ASH_EXPORT extern const char kForceAshToDesktop[];
#endif

// Returns true if items can be dragged off the shelf to unpin.
ASH_EXPORT bool UseDragOffShelf();

#if defined(OS_CHROMEOS)
// Returns true if a notification should appear when a low-power USB charger
// is connected.
ASH_EXPORT bool UseUsbChargerNotification();
#endif

}  // namespace switches
}  // namespace ash

#endif  // ASH_ASH_SWITCHES_H_
