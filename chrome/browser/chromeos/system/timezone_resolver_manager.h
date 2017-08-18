// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_TIMEZONE_RESOLVER_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_TIMEZONE_RESOLVER_MANAGER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/timezone/timezone_resolver.h"
#include "components/prefs/pref_change_registrar.h"

class PrefService;

namespace chromeos {
namespace system {

class TimeZoneResolverManager : public TimeZoneResolver::Delegate {
 public:
  TimeZoneResolverManager();
  ~TimeZoneResolverManager() override;

  // This sets primary_user_prefs_.
  void SetPrimaryUserPrefs(PrefService* pref_service);

  // TimeZoneResolver::Delegate overrides:
  bool ShouldSendWiFiGeolocationData() override;

  // TimeZoneResolver::Delegate overrides:
  bool ShouldSendCellularGeolocationData() override;

  // Starts or stops TimezoneResolver according to currect settings.
  void UpdateTimezoneResolver();

  // Returns true if result of timezone resolve should be applied to
  // system timezone (preferences might have changed since request was started).
  bool ShouldApplyResolvedTimezone();

  // Returns true if TimeZoneResolver should be running and taking in account
  // all configuration data.
  bool TimeZoneResolverShouldBeRunning();

 private:
  int GetTimezoneManagementSetting();

  // Local State initialization observer.
  void OnLocalStateInitialized(bool initialized);

  // This is non-null only after user logs in.
  PrefService* primary_user_prefs_ = nullptr;

  // This is used to subscribe to policy preference.
  PrefChangeRegistrar local_state_pref_change_registrar_;

  // True if initial policy values are loaded.
  bool local_state_initialized_ = false;

  // True if TimeZoneResolverManager may start/stop on its own.
  // Becomes true after UpdateTimezoneResolver() has been called at least once.
  bool initialized_ = false;

  base::WeakPtrFactory<TimeZoneResolverManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TimeZoneResolverManager);
};

}  // namespace system
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_TIMEZONE_RESOLVER_MANAGER_H_
