// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"

#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/features.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
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

  // This is only possible during unit tests. If the unit test didn't set the
  // local_state then it doesn't care about pref value and therefore we return
  // false.
  if (!g_browser_process->local_state()) {
    DLOG(WARNING) << "Local state has not been set and pref cannot be read";
    return false;
  }

#if !BUILDFLAG(ANDROID_JAVA_UI)
  return IsMetricsReportingEnabled(g_browser_process->local_state());
#else
  // Android currently obtain the value for whether the user has
  // obtain metrics reporting in non-standard ways.
  // TODO(gayane): Consolidate metric prefs on all platforms and eliminate this
  // special-case code, instead having all platforms go through the above flow.
  // http://crbug.com/362192, http://crbug.com/532084
  bool pref_value = false;

  pref_value = g_browser_process->local_state()->GetBoolean(
      prefs::kCrashReportingEnabled);
  return IsMetricsReportingEnabledWithPrefValue(pref_value);
#endif  // !BUILDFLAG(ANDROID_JAVA_UI)
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
