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
#include "chrome/common/pref_names.h"

namespace {

// Field trial IDs of the control and experiment groups. Though they are not
// literally "const", they are set only once, in Activate() below. See the .h
// file for what these groups represent.
int g_inactive = -1;
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
          "Instant", 1000, "Inactive", 2013, 7, 1, &g_inactive));

  // Try to give the user a consistent experience, if possible.
  if (base::FieldTrialList::IsOneTimeRandomizationEnabled())
    trial->UseOneTimeRandomization();

  g_instant = trial->AppendGroup("Instant",  10);  //  1%
  g_suggest = trial->AppendGroup("Suggest",  10);  //  1%
  g_hidden  = trial->AppendGroup("Hidden",  960);  // 96%
  g_silent  = trial->AppendGroup("Silent",   10);  //  1%
  g_control = trial->AppendGroup("Control",  10);  //  1%
}

// static
InstantFieldTrial::Group InstantFieldTrial::GetGroup(Profile* profile) {
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
    if (switch_value == switches::kInstantFieldTrialControl)
      return CONTROL;
    return INACTIVE;
  }

  const int group = base::FieldTrialList::FindValue("Instant");
  if (group == base::FieldTrial::kNotFinalized || group == g_inactive)
    return INACTIVE;

  // CONTROL and SILENT are unconstrained.
  if (group == g_control)
    return CONTROL;
  if (group == g_silent)
    return SILENT;

  // HIDDEN, SUGGEST and INSTANT need non-incognito, suggest-enabled profiles.
  const PrefService* prefs = profile ? profile->GetPrefs() : NULL;
  if (!prefs || profile->IsOffTheRecord() ||
      !prefs->GetBoolean(prefs::kSearchSuggestEnabled)) {
    return INACTIVE;
  }

  if (group == g_hidden)
    return HIDDEN;

  // SUGGEST and INSTANT require UMA opt-in.
  if (!MetricsServiceHelper::IsMetricsReportingEnabled())
    return INACTIVE;

  if (group == g_suggest)
    return SUGGEST;

  // Disable INSTANT for group policy overrides and explicit opt-out.
  if (prefs->IsManagedPreference(prefs::kInstantEnabled) ||
      prefs->GetBoolean(prefs::kInstantEnabledOnce)) {
    return INACTIVE;
  }

  if (group == g_instant)
    return INSTANT;

  NOTREACHED();
  return INACTIVE;
}

// static
bool InstantFieldTrial::IsInstantExperiment(Profile* profile) {
  Group group = GetGroup(profile);
  return group == INSTANT || group == SUGGEST || group == HIDDEN ||
         group == SILENT;
}

// static
bool InstantFieldTrial::IsHiddenExperiment(Profile* profile) {
  Group group = GetGroup(profile);
  return group == SUGGEST || group == HIDDEN || group == SILENT;
}

// static
bool InstantFieldTrial::IsSilentExperiment(Profile* profile) {
  Group group = GetGroup(profile);
  return group == SILENT;
}

// static
std::string InstantFieldTrial::GetGroupName(Profile* profile) {
  switch (GetGroup(profile)) {
    case INACTIVE: return std::string();
    case INSTANT:  return "_Instant";
    case SUGGEST:  return "_Suggest";
    case HIDDEN:   return "_Hidden";
    case SILENT:   return "_Silent";
    case CONTROL:  return "_Control";
  }

  NOTREACHED();
  return std::string();
}

// static
std::string InstantFieldTrial::GetGroupAsUrlParam(Profile* profile) {
  switch (GetGroup(profile)) {
    case INACTIVE: return std::string();
    case INSTANT:  return "ix=i9&";
    case SUGGEST:  return "ix=t9&";
    case HIDDEN:   return "ix=h9&";
    case SILENT:   return "ix=s9&";
    case CONTROL:  return "ix=c9&";
  }

  NOTREACHED();
  return std::string();
}

// static
bool InstantFieldTrial::ShouldSetSuggestedText(Profile* profile) {
  Group group = GetGroup(profile);
  return group != HIDDEN && group != SILENT;
}
