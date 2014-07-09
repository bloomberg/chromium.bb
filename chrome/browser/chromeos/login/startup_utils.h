// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_STARTUP_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_STARTUP_UTILS_H_

#include <string>

#include "base/callback.h"

class PrefRegistrySimple;

namespace chromeos {

// Static utility methods used at startup time to get/change bits of device
// state.
class StartupUtils {
 public:
  // Returns true if EULA has been accepted.
  static bool IsEulaAccepted();

  // Returns OOBE completion status.
  static bool IsOobeCompleted();

  // Marks EULA status as accepted.
  static void MarkEulaAccepted();

  // Marks OOBE process as completed.
  static void MarkOobeCompleted();

  // Stores the next pending OOBE screen in case it will need to be resumed.
  static void SaveOobePendingScreen(const std::string& screen);

  // Returns device registration completion status, i.e. second part of OOBE.
  static bool IsDeviceRegistered();

  // Marks device registered. i.e. second part of OOBE is completed.
  static void MarkDeviceRegistered(const base::Closure& done_callback);

  // Returns whether enrollment recovery is required.
  static bool IsEnrollmentRecoveryRequired();

  // Mark a device as requiring enrollment recovery.
  static void MarkEnrollmentRecoveryRequired();

  // Returns initial locale from local settings.
  static std::string GetInitialLocale();

  // Sets initial locale in local settings.
  static void SetInitialLocale(const std::string& locale);

  // Registers OOBE preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_STARTUP_UTILS_H_
