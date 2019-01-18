// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_ANDROID_SMS_ANDROID_SMS_APP_SETUP_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_ANDROID_SMS_ANDROID_SMS_APP_SETUP_CONTROLLER_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "url/gurl.h"

namespace extensions {
class Extension;
}  // namespace extensions

namespace chromeos {

namespace android_sms {

// Manages the setup and uninstallation process of the Android SMS PWA.
class AndroidSmsAppSetupController {
 public:
  AndroidSmsAppSetupController() = default;
  virtual ~AndroidSmsAppSetupController() = default;

  using SuccessCallback = base::OnceCallback<void(bool)>;

  // Performs the setup process for the app at |url|, which includes:
  //   (1) Installing the PWA,
  //   (2) Granting permission for the PWA to show notifications, and
  //   (3) Setting a cookie which defaults the PWA to remember this computer.
  virtual void SetUpApp(const GURL& url, SuccessCallback callback) = 0;

  // Returns the extension for the PWA at |url|; if no PWA exists, null is
  // returned.
  virtual const extensions::Extension* GetPwa(const GURL& url) = 0;

  // Deletes the cookie which causes the PWA to remember this computer by
  // default. Note that this does not actually stop the PWA from remembering
  // this computer; rather, it stops the PWA from *defaulting* to remember the
  // computer in the case that the user has not gone through the PWA's setup.
  virtual void DeleteRememberDeviceByDefaultCookie(
      const GURL& url,
      SuccessCallback callback) = 0;

  // Uninstalls the app at |url| and deletes relevant cookies from the setup
  // process.
  virtual void RemoveApp(const GURL& url, SuccessCallback callback) = 0;

 private:
  DISALLOW_COPY_AND_ASSIGN(AndroidSmsAppSetupController);
};

}  // namespace android_sms

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_ANDROID_SMS_ANDROID_SMS_APP_SETUP_CONTROLLER_H_
