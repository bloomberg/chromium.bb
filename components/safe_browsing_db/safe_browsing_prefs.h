// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Safe Browsing preferences and some basic utility functions for using them.

#ifndef COMPONENTS_SAFE_BROWSING_DB_SAFE_BROWSING_PREFS_H_
#define COMPONENTS_SAFE_BROWSING_DB_SAFE_BROWSING_PREFS_H_

#include "base/feature_list.h"

class PrefService;

namespace prefs {
// Boolean that tell us whether Safe Browsing extended reporting is enabled.
extern const char kSafeBrowsingExtendedReportingEnabled[];

// Boolean indicating whether Safe Browsing Scout reporting is enabled, which
// collects data for malware detection.
extern const char kSafeBrowsingScoutReportingEnabled[];

// Boolean indicating whether the Scout reporting workflow is enabled. This
// affects which of SafeBrowsingExtendedReporting or SafeBrowsingScoutReporting
// is used.
extern const char kSafeBrowsingScoutGroupSelected[];
}

namespace safe_browsing {

// When this feature is enabled, the Scout opt-in text will be displayed as of
// the next security incident. Until then, the legacy SBER text will appear.
extern const base::Feature kCanShowScoutOptIn;

// When this feature is enabled, the Scout opt-in text will immediately be
// displayed everywhere.
extern const base::Feature kOnlyShowScoutOptIn;

// Returns whether the currently active Safe Browsing Extended Reporting
// preference exists (eg: has been set before).
bool ExtendedReportingPrefExists(const PrefService& prefs);

// Returns the name of the Safe Browsing Extended Reporting pref that is
// currently in effect. The specific pref in-use may change through experiments.
const char* GetExtendedReportingPrefName(const PrefService& prefs);

// Returns whether Safe Browsing Extended Reporting is currently enabled.
// This should be used to decide if any of the reporting preferences are set,
// regardless of which specific one is set.
bool IsExtendedReportingEnabled(const PrefService& prefs);

// Updates UMA metrics about Safe Browsing Extended Reporting states.
void RecordExtendedReportingMetrics(const PrefService& prefs);

// Sets the currently active Safe Browsing Extended Reporting preference to the
// specified value.
void SetExtendedReportingPref(PrefService* prefs, bool value);

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_DB_SAFE_BROWSING_PREFS_H_
