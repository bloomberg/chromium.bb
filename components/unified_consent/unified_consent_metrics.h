// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNIFIED_CONSENT_UNIFIED_CONSENT_METRICS_H_
#define COMPONENTS_UNIFIED_CONSENT_UNIFIED_CONSENT_METRICS_H_

#include "components/unified_consent/unified_consent_service_client.h"

namespace unified_consent {

namespace metrics {

// Google services that can be toggled in user settings.
// Used in histograms. Do not change existing values, append new values at the
// end.
enum class SettingsHistogramValue {
  kNone = 0,
  kUnifiedConsentGiven = 1,
  kUserEvents = 2,
  kUrlKeyedAnonymizedDataCollection = 3,
  kSafeBrowsingExtendedReporting = 4,
  kSpellCheck = 5,
  kAllServicesWereEnabled = 6,

  kMaxValue = kAllServicesWereEnabled
};

// Records settings entries in the kSyncAndGoogleServicesSettingsHistogram.
// kNone is recorded when none of the settings is enabled.
void RecordSettingsHistogram(UnifiedConsentServiceClient* service_client,
                             PrefService* pref_service);

// Records a sample in the kSyncAndGoogleServicesSettingsHistogram. Wrapped in a
// function to avoid code size issues caused by histogram macros.
void RecordSettingsHistogramSample(SettingsHistogramValue value);

// Checks if a pref is enabled and if so, records a sample in the
// kSyncAndGoogleServicesSettingsHistogram. Returns true if a sample was
// recorded.
bool RecordSettingsHistogramFromPref(const char* pref_name,
                                     PrefService* pref_service,
                                     SettingsHistogramValue value);

// Checks if a service is enabled and if so, records a sample in the
// kSyncAndGoogleServicesSettingsHistogram. Returns true if a sample was
// recorded.
bool RecordSettingsHistogramFromService(
    UnifiedConsentServiceClient* client,
    UnifiedConsentServiceClient::Service service,
    SettingsHistogramValue value);

}  // namespace metrics

}  // namespace unified_consent

#endif  // COMPONENTS_UNIFIED_CONSENT_UNIFIED_CONSENT_METRICS_H_
