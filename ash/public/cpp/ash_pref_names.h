// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_ASH_PREF_NAMES_H_
#define ASH_PUBLIC_CPP_ASH_PREF_NAMES_H_

#include "ash/public/cpp/ash_public_export.h"

namespace ash {

namespace prefs {

ASH_PUBLIC_EXPORT extern const char kNightLightEnabled[];
ASH_PUBLIC_EXPORT extern const char kNightLightTemperature[];
ASH_PUBLIC_EXPORT extern const char kNightLightScheduleType[];
ASH_PUBLIC_EXPORT extern const char kNightLightCustomStartTime[];
ASH_PUBLIC_EXPORT extern const char kNightLightCustomEndTime[];

ASH_PUBLIC_EXPORT extern const char kShowLogoutButtonInTray[];
ASH_PUBLIC_EXPORT extern const char kLogoutDialogDurationMs[];

}  // namespace prefs

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_ASH_PREF_NAMES_H_
