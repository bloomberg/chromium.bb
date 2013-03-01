// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_PREFS_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_PREFS_H_

#include <string>
#include <vector>

namespace chromeos {

class KioskAppPrefsObserver;

// Interface to manage kiosk app preferences.
class KioskAppPrefs {
 public:
  typedef std::vector<std::string> AppIds;

  virtual ~KioskAppPrefs() {}

  // Gets/sets the app to auto launch at start up.
  virtual std::string GetAutoLaunchApp() const = 0;
  virtual void SetAutoLaunchApp(const std::string& app_id) = 0;

  // Gets/sets the auto launch suppression flag.
  virtual bool GetSuppressAutoLaunch() const = 0;
  virtual void SetSuppressAutoLaunch(bool suppress) = 0;

  // Access the kiosk app list.
  virtual void GetApps(AppIds* app_ids) const = 0;

  // Adds/removes a kiosk app by its id.
  virtual void AddApp(const std::string& app_id) = 0;
  virtual void RemoveApp(const std::string& app_id) = 0;

  virtual void AddObserver(KioskAppPrefsObserver* observer) = 0;
  virtual void RemoveObserver(KioskAppPrefsObserver* observer) = 0;
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_PREFS_H_
