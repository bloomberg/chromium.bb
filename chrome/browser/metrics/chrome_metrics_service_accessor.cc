// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"

#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/settings/cros_settings.h"
#endif

// static
bool ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled() {
  // TODO(blundell): Fix the unittests that don't set up the UI thread and
  // change this to just be DCHECK_CURRENTLY_ON().
  DCHECK(
      !content::BrowserThread::IsMessageLoopValid(content::BrowserThread::UI) ||
      content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
  return IsMetricsReportingEnabled(g_browser_process->local_state());
#else
  // ChromeOS and Android currently obtain the value for whether the user has
  // obtain metrics reporting in non-standard ways.
  // TODO(gayane): Consolidate metric prefs on all platforms and eliminate this
  // special-case code, instead having all platforms go through the above flow.
  // http://crbug.com/362192, http://crbug.com/532084
  bool pref_value = false;

#if defined(OS_CHROMEOS)
  // TODO(gayane): The check is added as a temporary fix for unittests. It's
  // not expected to happen from production code. This should be cleaned up
  // soon when metrics pref from cros will be eliminated.
  if (chromeos::CrosSettings::IsInitialized()) {
    chromeos::CrosSettings::Get()->GetBoolean(chromeos::kStatsReportingPref,
                                              &pref_value);
  }
#else  // Android
  pref_value = g_browser_process->local_state()->GetBoolean(
      prefs::kCrashReportingEnabled);
#endif  // defined(OS_CHROMEOS)

  return IsMetricsReportingEnabledWithPrefValue(pref_value);
#endif  // !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
}

// static
bool ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrial(
    const std::string& trial_name,
    const std::string& group_name) {
  return metrics::MetricsServiceAccessor::RegisterSyntheticFieldTrial(
      g_browser_process->metrics_service(), trial_name, group_name);
}

// static
bool ChromeMetricsServiceAccessor::RegisterSyntheticFieldTrialWithNameHash(
    uint32_t trial_name_hash,
    const std::string& group_name) {
  return metrics::MetricsServiceAccessor::
      RegisterSyntheticFieldTrialWithNameHash(
          g_browser_process->metrics_service(), trial_name_hash, group_name);
}
