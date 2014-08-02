// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "components/metrics/metrics_service.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/settings/cros_settings.h"
#endif

// static
bool ChromeMetricsServiceAccessor::IsMetricsReportingEnabled() {
  bool result = false;
  const PrefService* local_state = g_browser_process->local_state();
  if (local_state) {
    const PrefService::Preference* uma_pref =
        local_state->FindPreference(prefs::kMetricsReportingEnabled);
    if (uma_pref) {
      bool success = uma_pref->GetValue()->GetAsBoolean(&result);
      DCHECK(success);
    }
  }
  return result;
}

bool ChromeMetricsServiceAccessor::IsCrashReportingEnabled() {
#if defined(GOOGLE_CHROME_BUILD)
#if defined(OS_CHROMEOS)
  bool reporting_enabled = false;
  chromeos::CrosSettings::Get()->GetBoolean(chromeos::kStatsReportingPref,
                                            &reporting_enabled);
  return reporting_enabled;
#elif defined(OS_ANDROID)
  // Android has its own settings for metrics / crash uploading.
  const PrefService* prefs = g_browser_process->local_state();
  return prefs->GetBoolean(prefs::kCrashReportingEnabled);
#else
  return ChromeMetricsServiceAccessor::IsMetricsReportingEnabled();
#endif
#else
  return false;
#endif
}

// static
bool ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
    const std::string& trial, const std::string& group) {
  return MetricsServiceAccessor::RegisterSyntheticFieldTrial(
      g_browser_process->metrics_service(),
      trial,
      group);
}
