// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/histogram_macros.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing_db/safe_browsing_prefs.h"

namespace prefs {
const char kSafeBrowsingExtendedReportingEnabled[] =
    "safebrowsing.extended_reporting_enabled";
const char kSafeBrowsingScoutReportingEnabled[] =
    "safebrowsing.scout_reporting_enabled";
const char kSafeBrowsingScoutGroupSelected[] =
    "safebrowsing.scout_group_selected";
}  // namespace prefs

namespace safe_browsing {

bool ExtendedReportingPrefExists(const PrefService& prefs) {
  return prefs.HasPrefPath(GetExtendedReportingPrefName(prefs));
}

const char* GetExtendedReportingPrefName(const PrefService& prefs) {
  return prefs.GetBoolean(prefs::kSafeBrowsingScoutGroupSelected)
             ? prefs::kSafeBrowsingScoutReportingEnabled
             : prefs::kSafeBrowsingExtendedReportingEnabled;
}

bool IsExtendedReportingEnabled(const PrefService& prefs) {
  return prefs.GetBoolean(GetExtendedReportingPrefName(prefs));
}

void RecordExtendedReportingMetrics(const PrefService& prefs) {
  UMA_HISTOGRAM_BOOLEAN("SafeBrowsing.Pref.Extended",
                        IsExtendedReportingEnabled(prefs));
}

void SetExtendedReportingPref(PrefService* prefs, bool value) {
  prefs->SetBoolean(GetExtendedReportingPrefName(*prefs), value);
}

}  // namespace safe_browsing
