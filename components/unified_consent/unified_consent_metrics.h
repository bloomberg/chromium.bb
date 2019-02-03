// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_UNIFIED_CONSENT_UNIFIED_CONSENT_METRICS_H_
#define COMPONENTS_UNIFIED_CONSENT_UNIFIED_CONSENT_METRICS_H_

#include "components/unified_consent/unified_consent_service_client.h"

namespace syncer {
class SyncUserSettings;
}

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

// Sync data types that can be customized in settings.
// Used in histograms. Do not change existing values, append new values at the
// end.
enum class SyncDataType {
  kNone = 0,
  kApps = 1,
  kBookmarks = 2,
  kExtensions = 3,
  kHistory = 4,
  kSettings = 5,
  kThemes = 6,
  kTabs = 7,
  kPasswords = 8,
  kAutofill = 9,
  kPayments = 10,

  kMaxValue = kPayments
};

// Records settings entries in the SyncAndGoogleServicesSettings.
// kNone is recorded when none of the settings is enabled.
void RecordSettingsHistogram(UnifiedConsentServiceClient* service_client,
                             PrefService* pref_service);

// Records the sync data types that were turned off during the advanced sync
// opt-in flow. When none of the data types were turned off, kNone is recorded.
void RecordSyncSetupDataTypesHistrogam(syncer::SyncUserSettings* sync_settings,
                                       PrefService* pref_service);

}  // namespace metrics

}  // namespace unified_consent

#endif  // COMPONENTS_UNIFIED_CONSENT_UNIFIED_CONSENT_METRICS_H_
