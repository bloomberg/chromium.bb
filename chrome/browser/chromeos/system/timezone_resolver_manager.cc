// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system/timezone_resolver_manager.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/preferences.h"
#include "chrome/browser/chromeos/system/input_device_settings.h"
#include "chrome/browser/chromeos/system/timezone_util.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/prefs/pref_service.h"

namespace chromeos {
namespace system {

namespace {

// This is the result of several methods calculating configured
// time zone resolve processes.
enum ServiceConfiguration {
  UNSPECIFIED = 0,   // Try another configuration source.
  SHOULD_START = 1,  // This source requires service Start.
  SHOULD_STOP = 2,   // This source requires service Stop.
};

// Starts or stops TimezoneResolver if required by
// SystemTimezoneAutomaticDetectionPolicy.
// Returns SHOULD_* if timezone resolver status is controlled by this policy.
ServiceConfiguration GetServiceConfigurationFromAutomaticDetectionPolicy() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kDisableSystemTimezoneAutomaticDetectionPolicy)) {
    return UNSPECIFIED;
  }

  PrefService* local_state = g_browser_process->local_state();
  const bool is_managed = local_state->IsManagedPreference(
      prefs::kSystemTimezoneAutomaticDetectionPolicy);
  if (!is_managed)
    return UNSPECIFIED;

  int policy_value =
      local_state->GetInteger(prefs::kSystemTimezoneAutomaticDetectionPolicy);

  switch (policy_value) {
    case enterprise_management::SystemTimezoneProto::USERS_DECIDE:
      return UNSPECIFIED;
    case enterprise_management::SystemTimezoneProto::DISABLED:
      return SHOULD_STOP;
    case enterprise_management::SystemTimezoneProto::IP_ONLY:
      return SHOULD_START;
    case enterprise_management::SystemTimezoneProto::SEND_WIFI_ACCESS_POINTS:
      return SHOULD_START;
    case enterprise_management::SystemTimezoneProto::SEND_ALL_LOCATION_INFO:
      return SHOULD_START;
  }
  // Default for unknown policy value.
  NOTREACHED() << "Unrecognized policy value: " << policy_value;
  return SHOULD_STOP;
}

// Stops TimezoneResolver if SystemTimezonePolicy is applied.
// Returns SHOULD_* if timezone resolver status is controlled by this policy.
ServiceConfiguration GetServiceConfigurationFromSystemTimezonePolicy() {
  if (!HasSystemTimezonePolicy())
    return UNSPECIFIED;

  return SHOULD_STOP;
}

// Starts or stops TimezoneResolver if required by policy.
// Returns true if timezone resolver status is controlled by policy.
ServiceConfiguration GetServiceConfigurationFromPolicy() {
  ServiceConfiguration result =
      GetServiceConfigurationFromSystemTimezonePolicy();

  if (result != UNSPECIFIED)
    return result;

  result = GetServiceConfigurationFromAutomaticDetectionPolicy();
  return result;
}

// Returns service configuration for the user.
ServiceConfiguration GetServiceConfigurationFromUserPrefs(
    PrefService* user_prefs) {
  const bool value =
      user_prefs->GetBoolean(prefs::kResolveTimezoneByGeolocation);
  if (value)
    return SHOULD_START;

  return SHOULD_STOP;
}

// Returns service configuration for the signin screen.
ServiceConfiguration GetServiceConfigurationForSigninScreen() {
  const PrefService::Preference* device_pref =
      g_browser_process->local_state()->FindPreference(
          prefs::kResolveDeviceTimezoneByGeolocation);
  bool device_pref_value;
  if (!device_pref ||
      !device_pref->GetValue()->GetAsBoolean(&device_pref_value)) {
    // CfM devices default to static timezone.
    bool keyboard_driven_oobe =
        system::InputDeviceSettings::Get()->ForceKeyboardDrivenUINavigation();
    return keyboard_driven_oobe ? SHOULD_STOP : SHOULD_START;
  }

  // Do not start resolver if we are inside active user session.
  // If user preferences permit, it will be started on preferences
  // initialization.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kLoginUser))
    return SHOULD_STOP;

  return device_pref_value ? SHOULD_START : SHOULD_STOP;
}

}  // anonymous namespace.

TimeZoneResolverManager::TimeZoneResolverManager() : weak_factory_(this) {
  local_state_initialized_ =
      g_browser_process->local_state()->GetInitializationStatus() ==
      PrefService::INITIALIZATION_STATUS_SUCCESS;
  g_browser_process->local_state()->AddPrefInitObserver(
      base::Bind(&TimeZoneResolverManager::OnLocalStateInitialized,
                 weak_factory_.GetWeakPtr()));

  local_state_pref_change_registrar_.Init(g_browser_process->local_state());
  local_state_pref_change_registrar_.Add(
      prefs::kSystemTimezoneAutomaticDetectionPolicy,
      base::Bind(
          &::chromeos::system::TimeZoneResolverManager::UpdateTimezoneResolver,
          base::Unretained(this)));
}

TimeZoneResolverManager::~TimeZoneResolverManager() {}

void TimeZoneResolverManager::SetPrimaryUserPrefs(PrefService* pref_service) {
  primary_user_prefs_ = pref_service;
}

bool TimeZoneResolverManager::ShouldSendWiFiGeolocationData() {
  int timezone_setting = GetTimezoneManagementSetting();
  return (
      (timezone_setting ==
       enterprise_management::SystemTimezoneProto::SEND_WIFI_ACCESS_POINTS) ||
      (timezone_setting ==
       enterprise_management::SystemTimezoneProto::SEND_ALL_LOCATION_INFO));
}

bool TimeZoneResolverManager::ShouldSendCellularGeolocationData() {
  return (GetTimezoneManagementSetting() ==
          enterprise_management::SystemTimezoneProto::SEND_ALL_LOCATION_INFO);
}

int TimeZoneResolverManager::GetTimezoneManagementSetting() {
  PrefService* local_state = g_browser_process->local_state();
  const bool is_managed = local_state->IsManagedPreference(
      prefs::kSystemTimezoneAutomaticDetectionPolicy);
  if (!is_managed)
    return false;

  int policy_value =
      local_state->GetInteger(prefs::kSystemTimezoneAutomaticDetectionPolicy);

  DCHECK(policy_value <= enterprise_management::SystemTimezoneProto::
                             AutomaticTimezoneDetectionType_MAX);

  return policy_value;
}

void TimeZoneResolverManager::UpdateTimezoneResolver() {
  initialized_ = true;
  chromeos::TimeZoneResolver* resolver =
      g_browser_process->platform_part()->GetTimezoneResolver();
  // Local state becomes initialized when policy data is loaded,
  // and we need policies to decide whether resolver can be started.
  if (!local_state_initialized_) {
    resolver->Stop();
    return;
  }
  if (TimeZoneResolverShouldBeRunning())
    resolver->Start();
  else
    resolver->Stop();
}

bool TimeZoneResolverManager::ShouldApplyResolvedTimezone() {
  return TimeZoneResolverShouldBeRunning();
}

bool TimeZoneResolverManager::TimeZoneResolverShouldBeRunning() {
  ServiceConfiguration result = GetServiceConfigurationFromPolicy();

  if (result == UNSPECIFIED) {
    if (primary_user_prefs_) {
      result = GetServiceConfigurationFromUserPrefs(primary_user_prefs_);
    } else {
      // We are on a signin page.
      result = GetServiceConfigurationForSigninScreen();
    }
  }
  return result == SHOULD_START;
}

void TimeZoneResolverManager::OnLocalStateInitialized(bool initialized) {
  local_state_initialized_ = initialized;
  if (initialized_)
    UpdateTimezoneResolver();
}

}  // namespace system
}  // namespace chromeos
