// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/metrics_reporting_state.h"

#include "base/callback.h"
#include "base/metrics/histogram.h"
#include "base/prefs/pref_service.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/common/pref_names.h"
#include "chrome/installer/util/google_update_settings.h"
#include "components/metrics/metrics_service.h"
#include "content/public/browser/browser_thread.h"

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
#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
  g_browser_process->local_state()->SetBoolean(
      prefs::kMetricsReportingEnabled, updated_pref);
#endif
  if (to_update_pref == updated_pref) {
    RecordMetricsReportingHistogramValue(updated_pref ?
        METRICS_REPORTING_ENABLED : METRICS_REPORTING_DISABLED);
  } else {
    RecordMetricsReportingHistogramValue(METRICS_REPORTING_ERROR);
  }
  if (!callback_fn.is_null())
    callback_fn.Run(updated_pref);
}

} // namespace

void InitiateMetricsReportingChange(
    bool enabled,
    const OnMetricsReportingCallbackType& callback_fn) {
#if !defined(OS_CHROMEOS) && !defined(OS_ANDROID)
  if (!IsMetricsReportingUserChangable()) {
    if (!callback_fn.is_null()) {
      callback_fn.Run(
          ChromeMetricsServiceAccessor::IsMetricsReportingEnabled());
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

bool IsMetricsReportingUserChangable() {
  const PrefService* pref_service = g_browser_process->local_state();
  const PrefService::Preference* pref =
      pref_service->FindPreference(prefs::kMetricsReportingEnabled);
  return pref && !pref->IsManaged();
}
