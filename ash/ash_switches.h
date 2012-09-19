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
ASH_EXPORT extern const char kAshConstrainPointerToRoot[];
ASH_EXPORT extern const char kAshDebugShortcuts[];
ASH_EXPORT extern const char kAshDisableWorkspace2[];
ASH_EXPORT extern const char kAshDisableBootAnimation2[];
ASH_EXPORT extern const char kAshEnableAdvancedGestures[];
ASH_EXPORT extern const char kAshEnableOak[];
ASH_EXPORT extern const char kAshNotifyDisabled[];
ASH_EXPORT extern const char kAshSecondaryDisplayLayout[];
ASH_EXPORT extern const char kAshTouchHud[];
ASH_EXPORT extern const char kAshWindowAnimationsDisabled[];
ASH_EXPORT extern const char kAuraGoogleDialogFrames[];
ASH_EXPORT extern const char kAuraLegacyPowerButton[];
ASH_EXPORT extern const char kAuraNoShadows[];

}  // namespace switches
}  // namespace ash

#endif  // ASH_ASH_SWITCHES_H_
