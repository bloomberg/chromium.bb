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
ASH_EXPORT extern const char kAshDebugShowPreferredNetworks[];
ASH_EXPORT extern const char kAshDefaultGuestWallpaperLarge[];
ASH_EXPORT extern const char kAshDefaultGuestWallpaperSmall[];
ASH_EXPORT extern const char kAshDefaultWallpaperLarge[];
ASH_EXPORT extern const char kAshDefaultWallpaperSmall[];
#if defined(OS_CHROMEOS)
ASH_EXPORT extern const char kAshDisableAudioDeviceMenu[];
#endif
ASH_EXPORT extern const char kAshDisableAutoMaximizing[];
ASH_EXPORT extern const char kAshDisableAutoWindowPlacement[];
ASH_EXPORT extern const char kAshDisableDisplayChangeLimiter[];
ASH_EXPORT extern const char kAshDisableImmersiveFullscreen[];
ASH_EXPORT extern const char kAshDisableNewLockAnimations[];
ASH_EXPORT extern const char kAshDisablePerAppLauncher[];
ASH_EXPORT extern const char kAshDisableUIScaling[];
ASH_EXPORT extern const char kAshDisableDisplayRotation[];
ASH_EXPORT extern const char kAshDisableDragAndDropAppListToLauncher[];
#if defined(OS_CHROMEOS)
ASH_EXPORT extern const char kAshDisableSoftwareMirroring[];
ASH_EXPORT extern const char kAshDisableUsbChargerNotification[];
ASH_EXPORT extern const char kAshEnableAudioDeviceMenu[];
#endif
ASH_EXPORT extern const char kAshEnableAdvancedGestures[];
ASH_EXPORT extern const char kAshEnableBrightnessControl[];
ASH_EXPORT extern const char kAshEnableDockedWindows[];
#if defined(OS_LINUX)
ASH_EXPORT extern const char kAshEnableMemoryMonitor[];
#endif
ASH_EXPORT extern const char kAshEnableImmersiveFullscreen[];
ASH_EXPORT extern const char kAshEnableOak[];
ASH_EXPORT extern const char kAshEnableOverviewMode[];
ASH_EXPORT extern const char kAshEnableStickyEdges[];
ASH_EXPORT extern const char kAshEnableTrayDragging[];
ASH_EXPORT extern const char kAshForceMirrorMode[];
ASH_EXPORT extern const char kAshHideNotificationsForFactory[];
ASH_EXPORT extern const char kAshHostWindowBounds[];
ASH_EXPORT extern const char kAshImmersiveHideTabIndicators[];
ASH_EXPORT extern const char kAshSecondaryDisplayLayout[];
ASH_EXPORT extern const char kAshTouchHud[];
ASH_EXPORT extern const char kAshUseAlternateShelfLayout[];
ASH_EXPORT extern const char kAshUseFirstDisplayAsInternal[];
ASH_EXPORT extern const char kAuraLegacyPowerButton[];
#if defined(OS_WIN)
ASH_EXPORT extern const char kForceAshToDesktop[];
#endif
ASH_EXPORT extern const char kForcedMaximizeMode[];

// Returns true if the alternate shelf layout should be used.
ASH_EXPORT bool UseAlternateShelfLayout();

#if defined(OS_CHROMEOS)
// Returns true if new audio handler should be used.
ASH_EXPORT bool UseNewAudioHandler();

// Returns true if we should show the audio device switching UI.
ASH_EXPORT bool ShowAudioDeviceMenu();

// Returns true if a notification should appear when a low-power USB charger
// is connected.
ASH_EXPORT bool UseUsbChargerNotification();
#endif

}  // namespace switches
}  // namespace ash

#endif  // ASH_ASH_SWITCHES_H_
