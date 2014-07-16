// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_LAUNCH_ERROR_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_LAUNCH_ERROR_H_

#include <string>

#include "base/basictypes.h"

namespace chromeos {

class KioskAppLaunchError {
 public:
  enum Error {
    NONE,                     // No error.
    HAS_PENDING_LAUNCH,       // There is a pending launch already.
    CRYPTOHOMED_NOT_RUNNING,  // Unable to call cryptohome daemon.
    ALREADY_MOUNTED,          // Cryptohome is already mounted.
    UNABLE_TO_MOUNT,          // Unable to mount cryptohome.
    UNABLE_TO_REMOVE,         // Unable to remove cryptohome.
    UNABLE_TO_INSTALL,        // Unable to install app.
    USER_CANCEL,              // Canceled by user.
    NOT_KIOSK_ENABLED,        // Not a kiosk enabled app.
    UNABLE_TO_RETRIEVE_HASH,  // Unable to retrieve username hash.
    POLICY_LOAD_FAILED,       // Failed to load policy for kiosk account.
    UNABLE_TO_DOWNLOAD,       // Unalbe to download app's crx file.
    UNABLE_TO_LAUNCH,         // Unable to launch app.
  };

  // Returns a message for given |error|.
  static std::string GetErrorMessage(Error error);

  // Save error for displaying on next restart. Note only the last saved error
  // will be kept.
  static void Save(Error error);

  // Gets the saved error.
  static Error Get();

  // Clears the saved error.
  static void Clear();

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(KioskAppLaunchError);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_LAUNCH_ERROR_H_
