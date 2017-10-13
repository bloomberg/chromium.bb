// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASH_PREF_NAMES_H_
#define ASH_PUBLIC_CPP_ASH_PREF_NAMES_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

namespace prefs {

ASH_PUBLIC_EXPORT extern const char kAccessibilityLargeCursorEnabled[];
ASH_PUBLIC_EXPORT extern const char kAccessibilityLargeCursorDipSize[];
ASH_PUBLIC_EXPORT extern const char kAccessibilityStickyKeysEnabled[];
ASH_PUBLIC_EXPORT extern const char kAccessibilitySpokenFeedbackEnabled[];
ASH_PUBLIC_EXPORT extern const char kAccessibilityHighContrastEnabled[];
ASH_PUBLIC_EXPORT extern const char kAccessibilityScreenMagnifierCenterFocus[];
ASH_PUBLIC_EXPORT extern const char kAccessibilityScreenMagnifierEnabled[];
ASH_PUBLIC_EXPORT extern const char kAccessibilityScreenMagnifierScale[];
ASH_PUBLIC_EXPORT extern const char kAccessibilityVirtualKeyboardEnabled[];
ASH_PUBLIC_EXPORT extern const char kAccessibilityMonoAudioEnabled[];
ASH_PUBLIC_EXPORT extern const char kAccessibilityAutoclickEnabled[];
ASH_PUBLIC_EXPORT extern const char kAccessibilityAutoclickDelayMs[];
ASH_PUBLIC_EXPORT extern const char kAccessibilityCaretHighlightEnabled[];
ASH_PUBLIC_EXPORT extern const char kAccessibilityCursorHighlightEnabled[];
ASH_PUBLIC_EXPORT extern const char kAccessibilityFocusHighlightEnabled[];
ASH_PUBLIC_EXPORT extern const char kAccessibilitySelectToSpeakEnabled[];
ASH_PUBLIC_EXPORT extern const char kAccessibilitySwitchAccessEnabled[];
ASH_PUBLIC_EXPORT extern const char kShouldAlwaysShowAccessibilityMenu[];

ASH_PUBLIC_EXPORT extern const char kHasSeenStylus[];
ASH_PUBLIC_EXPORT extern const char kShownPaletteWelcomeBubble[];

ASH_PUBLIC_EXPORT extern const char kNightLightEnabled[];
ASH_PUBLIC_EXPORT extern const char kNightLightTemperature[];
ASH_PUBLIC_EXPORT extern const char kNightLightScheduleType[];
ASH_PUBLIC_EXPORT extern const char kNightLightCustomStartTime[];
ASH_PUBLIC_EXPORT extern const char kNightLightCustomEndTime[];

ASH_PUBLIC_EXPORT extern const char kShelfAlignment[];
ASH_PUBLIC_EXPORT extern const char kShelfAlignmentLocal[];
ASH_PUBLIC_EXPORT extern const char kShelfAutoHideBehavior[];
ASH_PUBLIC_EXPORT extern const char kShelfAutoHideBehaviorLocal[];
ASH_PUBLIC_EXPORT extern const char kShelfPreferences[];

ASH_PUBLIC_EXPORT extern const char kShowLogoutButtonInTray[];
ASH_PUBLIC_EXPORT extern const char kLogoutDialogDurationMs[];

ASH_PUBLIC_EXPORT extern const char kWallpaperColors[];

ASH_PUBLIC_EXPORT extern const char kUserBluetoothAdapterEnabled[];
ASH_PUBLIC_EXPORT extern const char kSystemBluetoothAdapterEnabled[];

ASH_PUBLIC_EXPORT extern const char kTouchpadEnabled[];
ASH_PUBLIC_EXPORT extern const char kTouchscreenEnabled[];

ASH_PUBLIC_EXPORT extern const char kQuickUnlockPinSalt[];

}  // namespace prefs

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASH_PREF_NAMES_H_
