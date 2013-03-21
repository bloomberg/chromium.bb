// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_LAUNCHER_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_LAUNCHER_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_launch_error.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

class Profile;

namespace chromeos {

// KioskAppLauncher launches a given app from login screen. It first attempts
// to mount a cryptohome for the app. If the mount is successful, it prepares
// app profile then calls StartupAppLauncher to finish the launch. If mount
// fails, it sets relevant launch error and restart chrome to gets back to
// the login screen. Note that there should only be one launch attempt in
// progress.
class KioskAppLauncher {
 public:
  explicit KioskAppLauncher(const std::string& app_id);

  // Starts a launch attempt. Fails immediately if there is already a launch
  // attempt running.
  void Start();

 private:
  class CryptohomedChecker;
  class ProfileLoader;

  // Private dtor because this class manages its own lifetime.
  ~KioskAppLauncher();

  void ReportLaunchResult(KioskAppLaunchError::Error error);

  void StartMount();
  void MountCallback(bool mount_success, cryptohome::MountError mount_error);

  void AttemptRemove();
  void RemoveCallback(bool success,
                      cryptohome::MountError return_code);

  void OnProfilePrepared(Profile* profile);

  // The instance of the current running launch.
  static KioskAppLauncher* running_instance_;

  const std::string app_id_;

  scoped_ptr<CryptohomedChecker> crytohomed_checker;
  scoped_ptr<ProfileLoader> profile_loader_;

  // Whether remove existing cryptohome has attempted.
  bool remove_attempted_;

  DISALLOW_COPY_AND_ASSIGN(KioskAppLauncher);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_LAUNCHER_H_
