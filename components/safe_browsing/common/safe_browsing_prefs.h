// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Safe Browsing preferences and some basic utility functions for using them.

#ifndef COMPONENTS_SAFE_BROWSING_COMMON_SAFE_BROWSING_PREFS_H_
#define COMPONENTS_SAFE_BROWSING_COMMON_SAFE_BROWSING_PREFS_H_

#include "base/feature_list.h"

class PrefRegistrySimple;
class PrefService;

namespace prefs {
// Boolean that tells us whether users are given the option to opt in to Safe
// Browsing extended reporting. This is exposed as a preference that can be
// overridden by enterprise policy.
extern const char kSafeBrowsingExtendedReportingOptInAllowed[];

// Boolean that tell us whether Safe Browsing extended reporting is enabled.
extern const char kSafeBrowsingExtendedReportingEnabled[];

// Boolean indicating whether Safe Browsing Scout reporting is enabled, which
// collects data for malware detection.
extern const char kSafeBrowsingScoutReportingEnabled[];

// Boolean indicating whether the Scout reporting workflow is enabled. This
// affects which of SafeBrowsingExtendedReporting or SafeBrowsingScoutReporting
// is used.
extern const char kSafeBrowsingScoutGroupSelected[];

// Boolean indicating whether the user has ever seen a security interstitial
// containing the legacy Extended Reporting opt-in.
extern const char kSafeBrowsingSawInterstitialExtendedReporting[];

// Boolean indicating whether the user has ever seen a security interstitial
// containing the new Scout opt-in.
extern const char kSafeBrowsingSawInterstitialScoutReporting[];
}

namespace safe_browsing {

// Command-line switch for changing the scout_group_selected preference. Should
// be set to either 'true' or 'false'. Primarily for testing purposes.
// TODO: this is temporary (crbug.com/662944)
extern const char kSwitchForceScoutGroup[];

// When this feature is enabled, the Scout opt-in text will be displayed as of
// the next security incident. Until then, the legacy SBER text will appear.
// TODO: this is temporary (crbug.com/662944)
extern const base::Feature kCanShowScoutOptIn;

// When this feature is enabled, the Scout opt-in text will immediately be
// displayed everywhere.
// TODO: this is temporary (crbug.com/662944)
extern const base::Feature kOnlyShowScoutOptIn;

// Enumerates the level of Safe Browsing Extended Reporting that is currently
// available.
enum ExtendedReportingLevel {
  // Extended reporting is off.
  SBER_LEVEL_OFF = 0,
  // The Legacy level of extended reporting is available, reporting happens in
  // response to security incidents.
  SBER_LEVEL_LEGACY = 1,
  // The Scout level of extended reporting is available, some data can be
  // collected to actively detect dangerous apps and sites.
  SBER_LEVEL_SCOUT = 2,
};

// Enumerates all the places where the Safe Browsing Extended Reporting
// preference can be changed.
// These values are written to logs.  New enum values can be added, but
// existing enums must never be renumbered or deleted and reused.
enum ExtendedReportingOptInLocation {
  // The chrome://settings UI (also shared with chrome://md-settings).
  SBER_OPTIN_SITE_CHROME_SETTINGS = 0,
  // The Android settings UI.
  SBER_OPTIN_SITE_ANDROID_SETTINGS = 1,
  // The Download Feedback popup.
  SBER_OPTIN_SITE_DOWNLOAD_FEEDBACK_POPUP = 2,
  // Any security interstitial (malware, SSL, etc).
  SBER_OPTIN_SITE_SECURITY_INTERSTITIAL = 3,
  // New sites must be added before SBER_OPTIN_SITE_MAX.
  SBER_OPTIN_SITE_MAX
};

// Determines which opt-in text should be used based on the currently active
// preference. Will return either |extended_reporting_pref| if the legacy
// Extended Reporting pref is active, or |scout_pref| if the Scout pref is
// active. Used for Android.
std::string ChooseOptInTextPreference(
    const PrefService& prefs,
    const std::string& extended_reporting_pref,
    const std::string& scout_pref);

// Determines which opt-in text should be used based on the currently active
// preference. Will return either |extended_reporting_resource| if the legacy
// Extended Reporting pref is active, or |scout_resource| if the Scout pref is
// active.
int ChooseOptInTextResource(const PrefService& prefs,
                            int extended_reporting_resource,
                            int scout_resource);

// Returns whether the currently active Safe Browsing Extended Reporting
// preference exists (eg: has been set before).
bool ExtendedReportingPrefExists(const PrefService& prefs);

// Returns the level of reporting available for the current user.
ExtendedReportingLevel GetExtendedReportingLevel(const PrefService& prefs);

// Returns the name of the Safe Browsing Extended Reporting pref that is
// currently in effect. The specific pref in-use may change through experiments.
const char* GetExtendedReportingPrefName(const PrefService& prefs);

// Initializes Safe Browsing preferences based on data such as experiment state,
// command line flags, etc.
// TODO: this is temporary (crbug.com/662944)
void InitializeSafeBrowsingPrefs(PrefService* prefs);

// Returns whether the user is able to modify the Safe Browsing Extended
// Reporting opt-in.
bool IsExtendedReportingOptInAllowed(const PrefService& prefs);

// Returns whether Safe Browsing Extended Reporting is currently enabled.
// This should be used to decide if any of the reporting preferences are set,
// regardless of which specific one is set.
bool IsExtendedReportingEnabled(const PrefService& prefs);

// Returns whether the currently-active Extended Reporting pref is Scout.
bool IsScout(const PrefService& prefs);

// Updates UMA metrics about Safe Browsing Extended Reporting states.
void RecordExtendedReportingMetrics(const PrefService& prefs);

// Registers user preferences related to Safe Browsing.
void RegisterProfilePrefs(PrefRegistrySimple* registry);

// Sets the currently active Safe Browsing Extended Reporting preference to the
// specified value. The |location| indicates the UI where the change was
// made.
void SetExtendedReportingPrefAndMetric(PrefService* prefs,
                                       bool value,
                                       ExtendedReportingOptInLocation location);
// This variant is used to simplify test code by omitting the location.
void SetExtendedReportingPref(PrefService* prefs, bool value);

// Called when a security interstitial is closed by the user.
// |on_show_pref_existed| indicates whether the pref existed when the
// interstitial was shown. |on_show_pref_value| contains the pref value when the
// interstitial was shown.
void UpdateMetricsAfterSecurityInterstitial(const PrefService& prefs,
                                            bool on_show_pref_existed,
                                            bool on_show_pref_value);

// Called to indicate that a security interstitial is about to be shown to the
// user. This may trigger the user to begin seeing the Scout opt-in text
// depending on their experiment state.
void UpdatePrefsBeforeSecurityInterstitial(PrefService* prefs);

}  // namespace safe_browsing

#endif  // COMPONENTS_SAFE_BROWSING_COMMON_SAFE_BROWSING_PREFS_H_
