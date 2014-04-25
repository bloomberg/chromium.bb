// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_DEMO_MODE_DEMO_APP_LAUNCHER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_DEMO_MODE_DEMO_APP_LAUNCHER_H_

#include <string>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/app_mode/kiosk_profile_loader.h"

namespace base {
class FilePath;
}

namespace chromeos {

// Class responsible for launching the demo app under a kiosk session.
class DemoAppLauncher : public KioskProfileLoader::Delegate {
 public:
  DemoAppLauncher();
  virtual ~DemoAppLauncher();

  void StartDemoAppLaunch();

  static bool IsDemoAppSession(const std::string& user_id);
  static void SetDemoAppPathForTesting(const base::FilePath& path);

  static const char kDemoUserName[];
  static const char kDemoAppId[];

 private:
  friend class DemoAppLauncherTest;

  // KioskProfileLoader::Delegate overrides:
  virtual void OnProfileLoaded(Profile* profile) OVERRIDE;
  virtual void OnProfileLoadFailed(KioskAppLaunchError::Error error) OVERRIDE;

  scoped_ptr<KioskProfileLoader> kiosk_profile_loader_;

  static base::FilePath* demo_app_path_;

  DISALLOW_COPY_AND_ASSIGN(DemoAppLauncher);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_DEMO_MODE_DEMO_APP_LAUNCHER_H_
