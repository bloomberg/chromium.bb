// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASH_SWITCHES_H_
#define ASH_ASH_SWITCHES_H_
#pragma once

#include "ash/ash_export.h"

namespace ash {
namespace switches {

// Note: If you add a switch, consider if it needs to be copied to a subsequent
// command line if the process executes a new copy of itself.  (For example,
// see chromeos::LoginUtil::GetOffTheRecordCommandLine().)

// Please keep alphabetized.
ASH_EXPORT extern const char kAshEnableOak[];
ASH_EXPORT extern const char kAuraGoogleDialogFrames[];
ASH_EXPORT extern const char kAuraLegacyPowerButton[];
ASH_EXPORT extern const char kAuraNoShadows[];
ASH_EXPORT extern const char kAuraNotify[];
ASH_EXPORT extern const char kAuraPanelManager[];
ASH_EXPORT extern const char kAuraWindowAnimationsDisabled[];
ASH_EXPORT extern const char kEnableAppListV2[];

}  // namespace switches
}  // namespace ash

#endif  // ASH_ASH_SWITCHES_H_
