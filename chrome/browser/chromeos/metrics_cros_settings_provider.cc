// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/metrics_cros_settings_provider.h"

#include "base/string_util.h"
#include "base/threading/thread_restrictions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/cros_settings.h"
#include "chrome/browser/chromeos/cros_settings_names.h"
#include "chrome/browser/chromeos/login/user_manager.h"
#include "chrome/browser/ui/options/options_util.h"
#include "chrome/installer/util/google_update_settings.h"

#if defined(USE_LINUX_BREAKPAD)
#include "chrome/app/breakpad_linux.h"
#endif

namespace chromeos {

void MetricsCrosSettingsProvider::DoSet(const std::string& path,
                                        Value* value) {
  DCHECK(path == kStatsReportingPref);
  bool enabled = false;
  CHECK(value->GetAsBoolean(&enabled));
  if (SetMetricsStatus(enabled)) {
    CrosSettings::Get()->FireObservers(path.c_str());
  }
}

bool MetricsCrosSettingsProvider::Get(const std::string& path,
                                      Value** value) const {
  DCHECK(path == kStatsReportingPref);
  *value = Value::CreateBooleanValue(GetMetricsStatus());
  return true;
}

// static
bool MetricsCrosSettingsProvider::SetMetricsStatus(bool enabled) {
  // We must be not logged in (on EULA page) or logged is as an owner to be able
  // to modify the setting.
  UserManager *user = UserManager::Get();
  if (user->user_is_logged_in() && !user->current_user_is_owner())
    return false;
  VLOG(1) << "Setting cros stats/crash metric reporting to " << enabled;
  bool collect_stats_consent = false;
  {
    // Loading consent file state causes us to do blocking IO on UI thread.
    // Temporarily allow it until we fix http://crbug.com/62626
    base::ThreadRestrictions::ScopedAllowIO allow_io;
    collect_stats_consent = GoogleUpdateSettings::GetCollectStatsConsent();
  }
  if (enabled != collect_stats_consent) {
    bool new_enabled = OptionsUtil::ResolveMetricsReportingEnabled(enabled);
#if defined(USE_LINUX_BREAKPAD)
    if (new_enabled)
      InitCrashReporter();
    // Else, if (!new_enabled), we should have turned crash reporting off
    // here, but there is no API for that currently (while we use
    // BreakPad). But this is not a big deal: crash reporting will be off
    // after reboot for the current process while other Chrome processes
    // will start when the setting is already set up. Other ChromeOS
    // processes does not use BreakPad.
#endif
    return new_enabled == enabled;
  }
  return false;
}

// static
bool MetricsCrosSettingsProvider::GetMetricsStatus() {
  // Loading consent file state causes us to do blocking IO on UI thread.
  // Temporarily allow it until we fix http://crbug.com/62626
  base::ThreadRestrictions::ScopedAllowIO allow_io;
  return GoogleUpdateSettings::GetCollectStatsConsent();
}

bool MetricsCrosSettingsProvider::HandlesSetting(const std::string& path) {
  return ::StartsWithASCII(path, kStatsReportingPref, true);
}

};  // namespace chromeos
