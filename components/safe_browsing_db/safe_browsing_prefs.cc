// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/metrics/histogram_macros.h"
#include "components/prefs/pref_service.h"
#include "components/safe_browsing_db/safe_browsing_prefs.h"

namespace {

// Switch value which force the ScoutGroupSelected pref to true.
const char kForceScoutGroupValueTrue[] = "true";
// Switch value which force the ScoutGroupSelected pref to false.
const char kForceScoutGroupValueFalse[] = "false";

// Returns if Scout is the currently-active pref.
bool IsScout(const std::string& pref) {
  return pref == prefs::kSafeBrowsingScoutReportingEnabled;
}

}  // namespace

namespace prefs {
const char kSafeBrowsingExtendedReportingEnabled[] =
    "safebrowsing.extended_reporting_enabled";
const char kSafeBrowsingScoutReportingEnabled[] =
    "safebrowsing.scout_reporting_enabled";
const char kSafeBrowsingScoutGroupSelected[] =
    "safebrowsing.scout_group_selected";
}  // namespace prefs

namespace safe_browsing {

const char kSwitchForceScoutGroup[] = "force-scout-group";

const base::Feature kCanShowScoutOptIn{"CanShowScoutOptIn",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kOnlyShowScoutOptIn{"OnlyShowScoutOptIn",
                                        base::FEATURE_DISABLED_BY_DEFAULT};

int ChooseOptInTextResource(const PrefService& prefs,
                            int extended_reporting_resource,
                            int scout_resource) {
  return IsScout(GetExtendedReportingPrefName(prefs))
             ? scout_resource
             : extended_reporting_resource;
}

bool ExtendedReportingPrefExists(const PrefService& prefs) {
  return prefs.HasPrefPath(GetExtendedReportingPrefName(prefs));
}

const char* GetExtendedReportingPrefName(const PrefService& prefs) {
  // The Scout pref is active if either of the experiment features are on, and
  // ScoutGroupSelected is on as well.
  if ((base::FeatureList::IsEnabled(kCanShowScoutOptIn) ||
       base::FeatureList::IsEnabled(kOnlyShowScoutOptIn)) &&
      prefs.GetBoolean(prefs::kSafeBrowsingScoutGroupSelected)) {
    return prefs::kSafeBrowsingScoutReportingEnabled;
  }

  // ..otherwise, either no experiment is on (ie: the Control group) or
  // ScoutGroupSelected is off. So we use the SBER pref instead.
  return prefs::kSafeBrowsingExtendedReportingEnabled;
}

void InitializeSafeBrowsingPrefs(PrefService* prefs) {
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          kSwitchForceScoutGroup)) {
    std::string switch_value =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            kSwitchForceScoutGroup);
    if (switch_value == kForceScoutGroupValueTrue) {
      prefs->SetBoolean(prefs::kSafeBrowsingScoutGroupSelected, true);
    } else if (switch_value == kForceScoutGroupValueFalse) {
      prefs->SetBoolean(prefs::kSafeBrowsingScoutGroupSelected, false);
    }
  }
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
