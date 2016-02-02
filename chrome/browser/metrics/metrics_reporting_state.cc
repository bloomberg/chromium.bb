// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/metrics_reporting_state.h"

#include "base/callback.h"
#include "base/metrics/histogram.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/installer/util/google_update_settings.h"
#include "components/metrics/metrics_pref_names.h"
#include "components/metrics/metrics_service.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_thread.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chromeos/settings/cros_settings_names.h"
#endif  // defined(OS_CHROMEOS)

namespace {

enum MetricsReportingChangeHistogramValue {
  METRICS_REPORTING_ERROR,
  METRICS_REPORTING_DISABLED,
  METRICS_REPORTING_ENABLED,
  METRICS_REPORTING_MAX
};

void RecordMetricsReportingHistogramValue(
    MetricsReportingChangeHistogramValue value) {
  UMA_HISTOGRAM_ENUMERATION(
      "UMA.MetricsReporting.Toggle", value, METRICS_REPORTING_MAX);
}

// Tries to set metrics reporting status to |enabled| and returns whatever is
// the result of the update.
bool SetGoogleUpdateSettings(bool enabled) {
  GoogleUpdateSettings::SetCollectStatsConsent(enabled);
  bool updated_pref = GoogleUpdateSettings::GetCollectStatsConsent();
  if (enabled != updated_pref)
    DVLOG(1) << "Unable to set metrics reporting status to " << enabled;

  return updated_pref;
}

// Does the necessary changes for MetricsReportingEnabled changes which needs
// to be done in the main thread.
// As arguments this function gets:
//  |to_update_pref| which indicates what the desired update should be,
//  |callback_fn| is the callback function to be called in the end
//  |updated_pref| is the result of attempted update.
// Update considers to be successful if |to_update_pref| and |updated_pref| are
// the same.
void SetMetricsReporting(bool to_update_pref,
                         const OnMetricsReportingCallbackType& callback_fn,
                         bool updated_pref) {
  metrics::MetricsService* metrics = g_browser_process->metrics_service();
  if (metrics) {
    if (updated_pref)
      metrics->Start();
    else
      metrics->Stop();
  }

#if !defined(OS_ANDROID)
  g_browser_process->local_state()->SetBoolean(
      metrics::prefs::kMetricsReportingEnabled, updated_pref);
#endif  // !defined(OS_ANDROID)

  // When a user opts in to the metrics reporting service, the previously
  // collected data should be cleared to ensure that nothing is reported before
  // a user opts in and all reported data is accurate.
  if (updated_pref && metrics)
    metrics->ClearSavedStabilityMetrics();

  if (to_update_pref == updated_pref) {
    RecordMetricsReportingHistogramValue(updated_pref ?
        METRICS_REPORTING_ENABLED : METRICS_REPORTING_DISABLED);
  } else {
    RecordMetricsReportingHistogramValue(METRICS_REPORTING_ERROR);
  }
  if (!callback_fn.is_null())
    callback_fn.Run(updated_pref);
}

#if defined(OS_CHROMEOS)
// Callback function for Chrome OS device settings change, so that the update is
// applied to metrics reporting state.
void OnDeviceSettingChange() {
  bool enable_metrics = false;
  chromeos::CrosSettings::Get()->GetBoolean(chromeos::kStatsReportingPref,
                                            &enable_metrics);
  InitiateMetricsReportingChange(enable_metrics,
                                 OnMetricsReportingCallbackType());
}
#endif

} // namespace

// TODO(gayane): Instead of checking policy before setting the metrics pref set
// the pref and register for notifications for the rest of the changes.
void InitiateMetricsReportingChange(
    bool enabled,
    const OnMetricsReportingCallbackType& callback_fn) {
#if !defined(OS_ANDROID)
  if (IsMetricsReportingPolicyManaged()) {
    if (!callback_fn.is_null()) {
      callback_fn.Run(
          ChromeMetricsServiceAccessor::IsMetricsAndCrashReportingEnabled());
    }
    return;
  }
#endif
  // Posts to FILE thread as SetGoogleUpdateSettings does IO operations.
  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::FILE,
      FROM_HERE,
      base::Bind(&SetGoogleUpdateSettings, enabled),
      base::Bind(&SetMetricsReporting, enabled, callback_fn));
}

bool IsMetricsReportingPolicyManaged() {
  const PrefService* pref_service = g_browser_process->local_state();
  const PrefService::Preference* pref =
      pref_service->FindPreference(metrics::prefs::kMetricsReportingEnabled);
  return pref && pref->IsManaged();
}

// TODO(gayane): Add unittest which will check that observer on device settings
// will trigger this function and kMetricsReportinEnabled as well as metrics
// service state will be updated accordingly.
void SetupMetricsStateForChromeOS() {
#if defined(OS_CHROMEOS)
  chromeos::CrosSettings::Get()->AddSettingsObserver(
      chromeos::kStatsReportingPref, base::Bind(&OnDeviceSettingChange));

  OnDeviceSettingChange();
#endif // defined(OS_CHROMEOS)
}
