// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_TIMEZONE_RESOLVER_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_TIMEZONE_RESOLVER_MANAGER_H_

#include "base/macros.h"
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

  // Returns the result of the provate call for tests.
  bool TimeZoneResolverShouldBeRunningForTests();

 private:
  // Returns true if TimeZoneResolver should be running and taking in account
  // all configuration data.
  bool TimeZoneResolverShouldBeRunning();

  int GetTimezoneManagementSetting();

  // This is non-null only after user logs in.
  PrefService* primary_user_prefs_;

  // This is used to subscribe to policy preference.
  PrefChangeRegistrar local_state_pref_change_registrar_;

  DISALLOW_COPY_AND_ASSIGN(TimeZoneResolverManager);
};

}  // namespace system
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_TIMEZONE_RESOLVER_MANAGER_H_
