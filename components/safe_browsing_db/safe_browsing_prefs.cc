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

std::string ChooseOptInTextPreference(
    const PrefService& prefs,
    const std::string& extended_reporting_pref,
    const std::string& scout_pref) {
  return IsScout(prefs) ? scout_pref : extended_reporting_pref;
}

int ChooseOptInTextResource(const PrefService& prefs,
                            int extended_reporting_resource,
                            int scout_resource) {
  return IsScout(prefs) ? scout_resource : extended_reporting_resource;
}

bool ExtendedReportingPrefExists(const PrefService& prefs) {
  return prefs.HasPrefPath(GetExtendedReportingPrefName(prefs));
}

ExtendedReportingLevel GetExtendedReportingLevel(const PrefService& prefs) {
  if (!IsExtendedReportingEnabled(prefs)) {
    return SBER_LEVEL_OFF;
  } else {
    return IsScout(prefs) ? SBER_LEVEL_SCOUT : SBER_LEVEL_LEGACY;
  }
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
  // First handle forcing kSafeBrowsingScoutGroupSelected pref via cmd-line.
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

    // If the switch is used, don't process the experiment state.
    return;
  }

  // Handle the three possible experiment states.
  if (base::FeatureList::IsEnabled(kOnlyShowScoutOptIn)) {
    // OnlyShowScoutOptIn immediately turns on ScoutGroupSelected pref.
    prefs->SetBoolean(prefs::kSafeBrowsingScoutGroupSelected, true);
  } else if (base::FeatureList::IsEnabled(kCanShowScoutOptIn)) {
    // CanShowScoutOptIn will only turn on ScoutGroupSelected pref if the legacy
    // SBER pref is false. Otherwise the legacy SBER pref will stay on and
    // continue to be used until the next security incident, at which point
    // the Scout pref will become the active one.
    if (!prefs->GetBoolean(prefs::kSafeBrowsingExtendedReportingEnabled)) {
      prefs->SetBoolean(prefs::kSafeBrowsingScoutGroupSelected, true);
    }
  } else {
    // Both experiment features are off, so this is the Control group. We must
    // handle the possibility that the user was previously in an experiment
    // group (above) that was reverted. We want to restore the user to a
    // reasonable state based on the ScoutGroup and ScoutReporting preferences.
    if (prefs->GetBoolean(prefs::kSafeBrowsingScoutReportingEnabled)) {
      // User opted-in to Scout which is broader than legacy Extended Reporting.
      // Opt them in to Extended Reporting.
      prefs->SetBoolean(prefs::kSafeBrowsingExtendedReportingEnabled, true);
    } else if (prefs->GetBoolean(prefs::kSafeBrowsingScoutGroupSelected)) {
      // User was in the Scout Group (ie: seeing the Scout opt-in text) but did
      // NOT opt-in to Scout. Assume this was a conscious choice and remove
      // their legacy Extended Reporting opt-in as well. The user will have a
      // chance to evaluate their choice next time they see the opt-in text.

      // We make the Extended Reporting pref mimic the state of the Scout
      // Reporting pref. So we either Clear it or set it to False.
      if (prefs->HasPrefPath(prefs::kSafeBrowsingScoutReportingEnabled)) {
        // Scout Reporting pref was explicitly set to false, so set the SBER
        // pref to false.
        prefs->SetBoolean(prefs::kSafeBrowsingExtendedReportingEnabled, false);
      } else {
        // Scout Reporting pref is unset, so clear the SBER pref.
        prefs->ClearPref(prefs::kSafeBrowsingExtendedReportingEnabled);
      }
    }

    // Also clear both the Scout settings to start over from a clean state and
    // avoid the above logic from triggering on next restart.
    prefs->ClearPref(prefs::kSafeBrowsingScoutGroupSelected);
    prefs->ClearPref(prefs::kSafeBrowsingScoutReportingEnabled);
  }
}

bool IsExtendedReportingEnabled(const PrefService& prefs) {
  return prefs.GetBoolean(GetExtendedReportingPrefName(prefs));
}

bool IsScout(const PrefService& prefs) {
  return GetExtendedReportingPrefName(prefs) ==
         prefs::kSafeBrowsingScoutReportingEnabled;
}

void RecordExtendedReportingMetrics(const PrefService& prefs) {
  // This metric tracks the extended browsing opt-in based on whichever setting
  // the user is currently seeing. It tells us whether extended reporting is
  // happening for this user.
  UMA_HISTOGRAM_BOOLEAN("SafeBrowsing.Pref.Extended",
                        IsExtendedReportingEnabled(prefs));

  // These metrics track the Scout transition.
  if (prefs.GetBoolean(prefs::kSafeBrowsingScoutGroupSelected)) {
    // Users in the Scout group: currently seeing the Scout opt-in.
    UMA_HISTOGRAM_BOOLEAN(
        "SafeBrowsing.Pref.Scout.ScoutGroup.SBER1Pref",
        prefs.GetBoolean(prefs::kSafeBrowsingExtendedReportingEnabled));
    UMA_HISTOGRAM_BOOLEAN(
        "SafeBrowsing.Pref.Scout.ScoutGroup.SBER2Pref",
        prefs.GetBoolean(prefs::kSafeBrowsingScoutReportingEnabled));
  } else {
    // Users not in the Scout group: currently seeing the SBER opt-in.
    UMA_HISTOGRAM_BOOLEAN(
        "SafeBrowsing.Pref.Scout.NoScoutGroup.SBER1Pref",
        prefs.GetBoolean(prefs::kSafeBrowsingExtendedReportingEnabled));
    // The following metric is a corner case. User was previously in the
    // Scout group and was able to opt-in to the Scout pref, but was since
    // removed from the Scout group (eg: by rolling back a Scout experiment).
    UMA_HISTOGRAM_BOOLEAN(
        "SafeBrowsing.Pref.Scout.NoScoutGroup.SBER2Pref",
        prefs.GetBoolean(prefs::kSafeBrowsingScoutReportingEnabled));
  }
}

void SetExtendedReportingPref(PrefService* prefs, bool value) {
  prefs->SetBoolean(GetExtendedReportingPrefName(*prefs), value);
}

void UpdatePrefsBeforeSecurityInterstitial(PrefService* prefs) {
  // Move the user into the Scout Group if the CanShowScoutOptIn experiment is
  // enabled.
  if (base::FeatureList::IsEnabled(kCanShowScoutOptIn)) {
    prefs->SetBoolean(prefs::kSafeBrowsingScoutGroupSelected, true);
  }
}

}  // namespace safe_browsing
