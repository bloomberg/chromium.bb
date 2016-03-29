// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/system/timezone_resolver_manager.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/preferences.h"
#include "chrome/browser/chromeos/system/timezone_util.h"
#include "chrome/common/pref_names.h"
#include "chromeos/chromeos_switches.h"
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
  if (!g_browser_process->local_state()->GetBoolean(
          prefs::kResolveDeviceTimezoneByGeolocation)) {
    return SHOULD_START;
  }

  // Do not start resolver if we are inside active user session.
  // If user preferences permit, it will be started on preferences
  // initialization.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(switches::kLoginUser))
    return SHOULD_STOP;

  return SHOULD_START;
}

}  // anonymous namespace.

TimeZoneResolverManager::TimeZoneResolverManager()
    : primary_user_prefs_(nullptr) {
}

TimeZoneResolverManager::~TimeZoneResolverManager() {}

void TimeZoneResolverManager::SetPrimaryUserPrefs(PrefService* pref_service) {
  primary_user_prefs_ = pref_service;
}

bool TimeZoneResolverManager::ShouldSendWiFiGeolocationData() {
  return false;
}

void TimeZoneResolverManager::UpdateTimezoneResolver() {
  if (TimeZoneResolverShouldBeRunning())
    g_browser_process->platform_part()->GetTimezoneResolver()->Start();
  else
    g_browser_process->platform_part()->GetTimezoneResolver()->Stop();
}

bool TimeZoneResolverManager::ShouldApplyResolvedTimezone() {
  return TimeZoneResolverShouldBeRunning();
}

bool TimeZoneResolverManager::TimeZoneResolverShouldBeRunning() {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          chromeos::switches::kDisableTimeZoneTrackingOption)) {
    return false;
  }
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

}  // namespace system
}  // namespace chromeos
