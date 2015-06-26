// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_CHROMEOS_DEVICETYPE_UTILS_H_
#define ASH_SYSTEM_CHROMEOS_DEVICETYPE_UTILS_H_

#include "ash/ash_export.h"
#include "base/strings/string16.h"

namespace ash {

// Assuming the given localization resources takes a device type parameter, this
// will substitute the appropriate device in (e.g. Chromebook, Chromebox).
ASH_EXPORT base::string16 SubstituteChromeOSDeviceType(int resource_id);

// Returns the name of the Chrome device type (e.g. Chromebook, Chromebox).
ASH_EXPORT base::string16 GetChromeOSDeviceName();

// Returns the resource ID for the current Chrome device type (e.g. Chromebook,
// Chromebox).
ASH_EXPORT int GetChromeOSDeviceTypeResourceId();

}  // namespace ash

#endif  // ASH_SYSTEM_CHROMEOS_DEVICETYPE_UTILS_H_
