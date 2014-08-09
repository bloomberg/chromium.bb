// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/settings/system_settings_provider.h"

#include "base/strings/string16.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chromeos/login/login_state.h"
#include "chromeos/settings/cros_settings_names.h"
#include "content/public/browser/render_process_host.h"

namespace chromeos {

SystemSettingsProvider::SystemSettingsProvider(
    const NotifyObserversCallback& notify_cb)
    : CrosSettingsProvider(notify_cb) {
  system::TimezoneSettings *timezone_settings =
      system::TimezoneSettings::GetInstance();
  timezone_settings->AddObserver(this);
  timezone_value_.reset(new base::StringValue(
      timezone_settings->GetCurrentTimezoneID()));
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
}

const base::Value* SystemSettingsProvider::Get(const std::string& path) const {
  if (path == kSystemTimezone)
    return timezone_value_.get();
  return NULL;
}

// The timezone is always trusted.
CrosSettingsProvider::TrustedStatus
    SystemSettingsProvider::PrepareTrustedValues(const base::Closure& cb) {
  return TRUSTED;
}

bool SystemSettingsProvider::HandlesSetting(const std::string& path) const {
  return path == kSystemTimezone;
}

void SystemSettingsProvider::TimezoneChanged(const icu::TimeZone& timezone) {
  // Fires system setting change notification.
  timezone_value_.reset(new base::StringValue(
      system::TimezoneSettings::GetTimezoneID(timezone)));
  NotifyObservers(kSystemTimezone);
}

}  // namespace chromeos
