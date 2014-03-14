// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POLICY_SERVER_BACKED_DEVICE_STATE_H_
#define CHROME_BROWSER_CHROMEOS_POLICY_SERVER_BACKED_DEVICE_STATE_H_

namespace policy {

// Dictionary key constants for prefs::kServerBackedDeviceState.
extern const char kDeviceStateManagementDomain[];
extern const char kDeviceStateRestoreMode[];

// Values for kDeviceStateRestoreMode.
extern const char kDeviceStateRestoreModeReEnrollmentEnforced[];
extern const char kDeviceStateRestoreModeReEnrollmentRequested[];

}  // namespace policy

#endif  // CHROME_BROWSER_CHROMEOS_POLICY_SERVER_BACKED_DEVICE_STATE_H_
