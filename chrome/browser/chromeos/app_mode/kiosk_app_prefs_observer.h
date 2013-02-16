// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_PREFS_OBSERVER_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_PREFS_OBSERVER_H_

namespace chromeos {

class KioskAppPrefsObserver {
 public:
  // Invoked when the auto launch app is changed.
  virtual void OnKioskAutoLaunchAppChanged() = 0;

  // Invoked when an app is added or removed from kiosk app list.
  virtual void OnKioskAppsChanged() = 0;

 protected:
  virtual ~KioskAppPrefsObserver() {}
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_PREFS_OBSERVER_H_
