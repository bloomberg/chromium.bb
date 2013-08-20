// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_PROFILE_LOADER_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_PROFILE_LOADER_H_

#include <string>

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_launch_error.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

class Profile;

namespace chromeos {

class KioskAppManager;

// KioskProfileLoader loads a special profile for a given app. It first attempts
// to mount a cryptohome for the app. If the mount is successful, it prepares
// app profile then calls the delegate.
class KioskProfileLoader {
 public:
  class Delegate {
   public:
    virtual void OnProfileLoaded(Profile* profile) = 0;
    virtual void OnProfileLoadFailed(KioskAppLaunchError::Error error) = 0;

   protected:
    virtual ~Delegate() {}
  };

  KioskProfileLoader(KioskAppManager* kiosk_app_manager,
                     const std::string& app_id,
                     Delegate* delegate);

  ~KioskProfileLoader();

  // Starts profile load. Calls delegate on success or failure.
  void Start();

 private:
  class CryptohomedChecker;
  class ProfileLoader;

  void ReportLaunchResult(KioskAppLaunchError::Error error);

  void StartMount();
  void MountCallback(bool mount_success, cryptohome::MountError mount_error);

  void AttemptRemove();
  void RemoveCallback(bool success,
                      cryptohome::MountError return_code);

  void OnProfilePrepared(Profile* profile);

  KioskAppManager* kiosk_app_manager_;
  const std::string app_id_;
  std::string user_id_;
  Delegate* delegate_;

  scoped_ptr<CryptohomedChecker> crytohomed_checker;
  scoped_ptr<ProfileLoader> profile_loader_;

  // Whether remove existing cryptohome has attempted.
  bool remove_attempted_;

  DISALLOW_COPY_AND_ASSIGN(KioskProfileLoader);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_PROFILE_LOADER_H_
