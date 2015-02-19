// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"

#include "base/command_line.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/metrics_services_manager.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/variations/metrics_util.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/settings/cros_settings.h"
#endif

// static
// TODO(asvitkine): This function does not report the correct value on Android,
// see http://crbug.com/362192.
bool ChromeMetricsServiceAccessor::IsMetricsReportingEnabled() {
  // If the user permits metrics reporting with the checkbox in the
  // prefs, we turn on recording.  We disable metrics completely for
  // non-official builds, or when field trials are forced.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kForceFieldTrials)) {
    return false;
  }

  bool enabled = false;
#if defined(GOOGLE_CHROME_BUILD)
#if defined(OS_CHROMEOS)
  chromeos::CrosSettings::Get()->GetBoolean(chromeos::kStatsReportingPref,
                                            &enabled);
#else
  enabled = g_browser_process->local_state()->
      GetBoolean(prefs::kMetricsReportingEnabled);
#endif  // #if defined(OS_CHROMEOS)
#endif  // defined(GOOGLE_CHROME_BUILD)
  return enabled;
}

bool ChromeMetricsServiceAccessor::IsCrashReportingEnabled() {
#if defined(GOOGLE_CHROME_BUILD)
#if defined(OS_ANDROID)
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
    const std::string& trial_name,
    const std::string& group_name) {
  return RegisterSyntheticFieldTrialWithNameHash(metrics::HashName(trial_name),
                                                 group_name);
}

// static
bool ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrialWithNameHash(
    uint32_t trial_name_hash,
    const std::string& group_name) {
  return metrics::MetricsServiceAccessor::RegisterSyntheticFieldTrial(
      g_browser_process->metrics_service(),
      trial_name_hash,
      metrics::HashName(group_name));
}
