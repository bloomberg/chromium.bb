// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_PREFS_LOCAL_STATE_H_
#define CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_PREFS_LOCAL_STATE_H_

#include <string>

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/app_mode/kiosk_app_prefs.h"

class PrefService;

namespace chromeos {

class KioskAppManager;

// Implements KioskAppPrefs persisted in local state.
class KioskAppPrefsLocalState : public KioskAppPrefs {
 public:
  // KioskAppPrefs overrides:
  virtual std::string GetAutoLaunchApp() const OVERRIDE;
  virtual void SetAutoLaunchApp(const std::string& app_id) OVERRIDE;
  virtual bool GetSuppressAutoLaunch() const OVERRIDE;
  virtual void SetSuppressAutoLaunch(bool suppress) OVERRIDE;
  virtual void GetApps(AppIds* app_ids) const OVERRIDE;
  virtual void AddApp(const std::string& app_id) OVERRIDE;
  virtual void RemoveApp(const std::string& app_id) OVERRIDE;
  virtual void AddObserver(KioskAppPrefsObserver* observer) OVERRIDE;
  virtual void RemoveObserver(KioskAppPrefsObserver* observer) OVERRIDE;

 private:
  friend class KioskAppManager;

  KioskAppPrefsLocalState();
  virtual ~KioskAppPrefsLocalState();

  // Returns true if |app_id| is in prefs.
  bool HasApp(const std::string& app_id) const;

  PrefService* local_state_;  // Not owned.
  ObserverList<KioskAppPrefsObserver, true> observers_;

  DISALLOW_COPY_AND_ASSIGN(KioskAppPrefsLocalState);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_APP_MODE_KIOSK_APP_PREFS_LOCAL_STATE_H_
