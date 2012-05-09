// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/instant/instant_field_trial.h"

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/metrics/experiments_helper.h"
#include "chrome/common/metrics/variation_ids.h"
#include "chrome/common/pref_names.h"

namespace {

// IDs of the field trial groups. Though they are not literally "const", they
// are set only once, in Activate() below.
int g_instant = 0;
int g_suggest = 0;
int g_hidden  = 0;
int g_silent  = 0;
int g_control = 0;

}

// static
void InstantFieldTrial::Activate() {
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial(
          "Instant", 1000, "CONTROL", 2013, 7, 1, &g_control));

  // Try to give the user a consistent experience, if possible.
  if (base::FieldTrialList::IsOneTimeRandomizationEnabled())
    trial->UseOneTimeRandomization();

  // Though each group (including CONTROL) is nominally at 20%, GetMode()
  // often returns SILENT. See below for details.
  g_instant = trial->AppendGroup("INSTANT", 200);  // 20%
  g_suggest = trial->AppendGroup("SUGGEST", 200);  // 20%
  g_hidden  = trial->AppendGroup("HIDDEN",  200);  // 20%
  g_silent  = trial->AppendGroup("SILENT",  200);  // 20%

  // Mark these groups in requests to Google.
  experiments_helper::AssociateGoogleVariationID(
      "Instant", "CONTROL", chrome_variations::kInstantIDControl);
  experiments_helper::AssociateGoogleVariationID(
      "Instant", "SILENT", chrome_variations::kInstantIDSilent);
  experiments_helper::AssociateGoogleVariationID(
      "Instant", "HIDDEN", chrome_variations::kInstantIDHidden);
  experiments_helper::AssociateGoogleVariationID(
      "Instant", "SUGGEST", chrome_variations::kInstantIDSuggest);
  experiments_helper::AssociateGoogleVariationID(
      "Instant", "INSTANT", chrome_variations::kInstantIDInstant);
}

// static
InstantFieldTrial::Mode InstantFieldTrial::GetMode(Profile* profile) {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kInstantFieldTrial)) {
    std::string switch_value =
        command_line->GetSwitchValueASCII(switches::kInstantFieldTrial);
    if (switch_value == switches::kInstantFieldTrialInstant)
      return INSTANT;
    if (switch_value == switches::kInstantFieldTrialSuggest)
      return SUGGEST;
    if (switch_value == switches::kInstantFieldTrialHidden)
      return HIDDEN;
    if (switch_value == switches::kInstantFieldTrialSilent)
      return SILENT;
    return CONTROL;
  }

  // Instant explicitly enabled in chrome://settings.
  const PrefService* prefs = profile ? profile->GetPrefs() : NULL;
  if (prefs && prefs->GetBoolean(prefs::kInstantEnabled))
    return INSTANT;

  const int group = base::FieldTrialList::FindValue("Instant");
  if (group == base::FieldTrial::kNotFinalized || group == g_control)
    return CONTROL;

  // HIDDEN, SUGGEST and INSTANT need non-incognito, suggest-enabled, UMA
  // opted-in profiles.
  if (prefs && !profile->IsOffTheRecord() &&
      prefs->GetBoolean(prefs::kSearchSuggestEnabled) &&
      MetricsServiceHelper::IsMetricsReportingEnabled()) {
    if (group == g_hidden)
      return HIDDEN;
    if (group == g_suggest)
      return SUGGEST;

    // INSTANT also requires no group policy override and no explicit opt-out.
    if (!prefs->IsManagedPreference(prefs::kInstantEnabled) &&
        !prefs->GetBoolean(prefs::kInstantEnabledOnce)) {
      if (group == g_instant)
        return INSTANT;
    }
  }

  // Default is SILENT. This will be returned for most users (for example, even
  // if a user falls into another group, but has suggest or UMA disabled).
  return SILENT;
}

// static
std::string InstantFieldTrial::GetModeAsString(Profile* profile) {
  const Mode mode = GetMode(profile);
  switch (mode) {
    case INSTANT: return "_Instant";
    case SUGGEST: return "_Suggest";
    case HIDDEN:  return "_Hidden";
    case SILENT:  return "_Silent";
    case CONTROL: return std::string();
  }

  NOTREACHED();
  return std::string();
}
