// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_STARTUP_UTILS_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_STARTUP_UTILS_H_

#include <string>

#include "base/callback.h"

class PrefRegistrySimple;

namespace base {
class TimeDelta;
}

namespace chromeos {

// Static utility methods used at startup time to get/change bits of device
// state.
class StartupUtils {
 public:
  // Returns true if EULA has been accepted.
  static bool IsEulaAccepted();

  // Returns OOBE completion status, i.e. whether the OOBE wizard should be run
  // on next boot.  This is NOT what causes the .oobe_completed flag file to be
  // written.
  static bool IsOobeCompleted();

  // Marks EULA status as accepted.
  static void MarkEulaAccepted();

  // Marks OOBE process as completed.
  static void MarkOobeCompleted();

  // Stores the next pending OOBE screen in case it will need to be resumed.
  static void SaveOobePendingScreen(const std::string& screen);

  // Returns the time since the OOBE flag file was created.
  static base::TimeDelta GetTimeSinceOobeFlagFileCreation();

  // Returns device registration completion status, i.e. second part of OOBE.
  // It is triggered by enrolling the device, but also by logging in as a
  // consumer owner or by logging in as guest.  This state change is announced
  // to the system by writing the .oobe_completed flag file.
  static bool IsDeviceRegistered();

  // Marks device registered. i.e. second part of OOBE is completed.
  static void MarkDeviceRegistered(const base::Closure& done_callback);

  // Mark a device as requiring enrollment recovery.
  static void MarkEnrollmentRecoveryRequired();

  // Returns initial locale from local settings.
  static std::string GetInitialLocale();

  // Sets initial locale in local settings.
  static void SetInitialLocale(const std::string& locale);

  // Saves the time of the last update check which did not result in any update.
  static void SaveTimeOfLastUpdateCheckWithoutUpdate(base::Time time);

  // Clears the update check time which was previously saved.
  static void ClearTimeOfLastUpdateCheckWithoutUpdate();

  // Returns the time of the last update check which did not lead to an update.
  static base::Time GetTimeOfLastUpdateCheckWithoutUpdate();

  // Registers OOBE preferences.
  static void RegisterPrefs(PrefRegistrySimple* registry);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_STARTUP_UTILS_H_
