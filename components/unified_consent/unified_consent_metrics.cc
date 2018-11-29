// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/unified_consent/unified_consent_metrics.h"

#include "base/metrics/histogram_macros.h"
#include "components/prefs/pref_service.h"
#include "components/unified_consent/pref_names.h"

namespace {

// Histogram name for the consent bump action.
const char kConsentBumpActionMetricName[] = "UnifiedConsent.ConsentBump.Action";

// Histogram recorded at startup to log which Google services are enabled.
const char kSyncAndGoogleServicesSettingsHistogram[] =
    "UnifiedConsent.SyncAndGoogleServicesSettings";

}  // namespace

namespace unified_consent {

namespace metrics {

void RecordConsentBumpMetric(UnifiedConsentBumpAction action) {
  UMA_HISTOGRAM_ENUMERATION(
      kConsentBumpActionMetricName, action,
      UnifiedConsentBumpAction::kUnifiedConsentBumpActionMoreOptionsMax);
}

void RecordConsentBumpEligibility(bool eligible) {
  UMA_HISTOGRAM_BOOLEAN("UnifiedConsent.ConsentBump.EligibleAtStartup",
                        eligible);
}

void RecordSettingsHistogram(UnifiedConsentServiceClient* service_client,
                             PrefService* pref_service) {
  bool metric_recorded = false;

  metric_recorded |= RecordSettingsHistogramFromPref(
      prefs::kAllUnifiedConsentServicesWereEnabled, pref_service,
      metrics::SettingsHistogramValue::kAllServicesWereEnabled);
  metric_recorded |= RecordSettingsHistogramFromPref(
      prefs::kUrlKeyedAnonymizedDataCollectionEnabled, pref_service,
      metrics::SettingsHistogramValue::kUrlKeyedAnonymizedDataCollection);
  metric_recorded |= RecordSettingsHistogramFromService(
      service_client,
      UnifiedConsentServiceClient::Service::kSafeBrowsingExtendedReporting,
      metrics::SettingsHistogramValue::kSafeBrowsingExtendedReporting);
  metric_recorded |= RecordSettingsHistogramFromService(
      service_client, UnifiedConsentServiceClient::Service::kSpellCheck,
      metrics::SettingsHistogramValue::kSpellCheck);

  if (!metric_recorded)
    RecordSettingsHistogramSample(metrics::SettingsHistogramValue::kNone);
}

void RecordSettingsHistogramSample(SettingsHistogramValue value) {
  UMA_HISTOGRAM_ENUMERATION(kSyncAndGoogleServicesSettingsHistogram, value);
}

bool RecordSettingsHistogramFromPref(const char* pref_name,
                                     PrefService* pref_service,
                                     SettingsHistogramValue value) {
  if (!pref_service->GetBoolean(pref_name))
    return false;
  RecordSettingsHistogramSample(value);
  return true;
}

bool RecordSettingsHistogramFromService(
    UnifiedConsentServiceClient* client,
    UnifiedConsentServiceClient::Service service,
    SettingsHistogramValue value) {
  if (client->GetServiceState(service) !=
      UnifiedConsentServiceClient::ServiceState::kEnabled) {
    return false;
  }

  RecordSettingsHistogramSample(value);
  return true;
}

}  // namespace metrics

}  // namespace unified_consent
