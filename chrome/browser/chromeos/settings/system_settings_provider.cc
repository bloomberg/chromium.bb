// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/system_settings_provider.h"

#include "base/strings/string16.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chromeos/system/timezone_util.h"
#include "chromeos/login/login_state.h"
#include "chromeos/settings/cros_settings_names.h"

namespace chromeos {

SystemSettingsProvider::SystemSettingsProvider(
    const NotifyObserversCallback& notify_cb)
    : CrosSettingsProvider(notify_cb) {
  system::TimezoneSettings *timezone_settings =
      system::TimezoneSettings::GetInstance();
  timezone_settings->AddObserver(this);
  timezone_value_.reset(
      new base::Value(timezone_settings->GetCurrentTimezoneID()));
  per_user_timezone_enabled_value_.reset(
      new base::Value(system::PerUserTimezoneEnabled()));
}

SystemSettingsProvider::~SystemSettingsProvider() {
  system::TimezoneSettings::GetInstance()->RemoveObserver(this);
}

void SystemSettingsProvider::DoSet(const std::string& path,
                                   const base::Value& in_value) {
  // Only non-guest users can change the time zone.
  if (LoginState::Get()->IsGuestSessionUser() ||
      LoginState::Get()->IsPublicSessionUser()) {
    return;
  }

  if (path == kSystemTimezone) {
    base::string16 timezone_id;
    if (!in_value.GetAsString(&timezone_id))
      return;
    // This will call TimezoneChanged.
    system::TimezoneSettings::GetInstance()->SetTimezoneFromID(timezone_id);
  }
  // kPerUserTimezoneEnabled is read-only.
}

const base::Value* SystemSettingsProvider::Get(const std::string& path) const {
  if (path == kSystemTimezone)
    return timezone_value_.get();

  if (path == kPerUserTimezoneEnabled)
    return per_user_timezone_enabled_value_.get();

  return NULL;
}

// The timezone is always trusted.
CrosSettingsProvider::TrustedStatus
    SystemSettingsProvider::PrepareTrustedValues(const base::Closure& cb) {
  return TRUSTED;
}

bool SystemSettingsProvider::HandlesSetting(const std::string& path) const {
  return path == kSystemTimezone || path == kPerUserTimezoneEnabled;
}

void SystemSettingsProvider::TimezoneChanged(const icu::TimeZone& timezone) {
  // Fires system setting change notification.
  timezone_value_.reset(
      new base::Value(system::TimezoneSettings::GetTimezoneID(timezone)));
  NotifyObservers(kSystemTimezone);
}

}  // namespace chromeos
