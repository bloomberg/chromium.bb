// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SCREENSAVER_SCREENSAVER_CONTROLLER_H_
#define CHROME_BROWSER_CHROMEOS_SCREENSAVER_SCREENSAVER_CONTROLLER_H_

#include <string>

#include "ash/wm/user_activity_observer.h"
#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/time.h"
#include "chromeos/dbus/power_manager_client.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace extensions {
class Extension;
}

namespace chromeos {

// This class controls the management of the screensaver extension. It is
// responsible for:
// . Enabling and disabling of the screensaver, along with ensuring that we
//   have only one screensaver at a time.
// . Managing the showing and hiding of the current screensaver based on user
//   activity.
// . Any power management that may be required while a screensaver is active.
class ScreensaverController : public ash::UserActivityObserver,
                              public PowerManagerClient::Observer,
                              public content::NotificationObserver {
 public:
  ScreensaverController();
  virtual ~ScreensaverController();

 private:
  FRIEND_TEST_ALL_PREFIXES(ScreensaverControllerTest, Basic);
  FRIEND_TEST_ALL_PREFIXES(ScreensaverControllerTest, OutOfOrder);

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Overridden from PowerManagerClient::Observer:
  virtual void IdleNotify(int64 threshold) OVERRIDE;

  // UserActivityObserver::Observer overrides:
  virtual void OnUserActivity() OVERRIDE;

  void SetupScreensaver(const std::string& screensaver_extension_id);
  void TeardownScreensaver();

  void RequestNextIdleNotification();

  std::string screensaver_extension_id_;

  content::NotificationRegistrar registrar_;

  base::TimeDelta threshold_;

  base::WeakPtrFactory<ScreensaverController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(ScreensaverController);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SCREENSAVER_SCREENSAVER_CONTROLLER_H_
