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

  int group = 0;
  if (trial->group() == g_instant)
    group = 1;
  else if (trial->group() == g_suggest)
    group = 2;
  else if (trial->group() == g_hidden)
    group = 3;
  else if (trial->group() == g_silent)
    group = 4;
  else if (trial->group() == g_control)
    group = 5;
  UMA_HISTOGRAM_ENUMERATION("Instant.FieldTrial.Size", group, 6);
}

// static
InstantFieldTrial::Group InstantFieldTrial::GetGroup(Profile* profile) {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kInstantFieldTrial)) {
    std::string switch_value =
        command_line->GetSwitchValueASCII(switches::kInstantFieldTrial);
    Group group = INACTIVE;
    if (switch_value == switches::kInstantFieldTrialInstant)
      group = INSTANT;
    else if (switch_value == switches::kInstantFieldTrialSuggest)
      group = SUGGEST;
    else if (switch_value == switches::kInstantFieldTrialHidden)
      group = HIDDEN;
    else if (switch_value == switches::kInstantFieldTrialSilent)
      group = SILENT;
    else if (switch_value == switches::kInstantFieldTrialControl)
      group = CONTROL;
    UMA_HISTOGRAM_ENUMERATION("Instant.FieldTrial.Reason", 1, 10);
    return group;
  }

  const int group = base::FieldTrialList::FindValue("Instant");
  if (group == base::FieldTrial::kNotFinalized || group == g_inactive) {
    UMA_HISTOGRAM_ENUMERATION("Instant.FieldTrial.Reason", 2, 10);
    return INACTIVE;
  }

  const PrefService* prefs = profile ? profile->GetPrefs() : NULL;
  if (!prefs) {
    UMA_HISTOGRAM_ENUMERATION("Instant.FieldTrial.Reason", 3, 10);
    return INACTIVE;
  }

  if (profile->IsOffTheRecord()) {
    UMA_HISTOGRAM_ENUMERATION("Instant.FieldTrial.Reason", 4, 10);
    return INACTIVE;
  }

  if (prefs->GetBoolean(prefs::kInstantEnabledOnce)) {
    UMA_HISTOGRAM_ENUMERATION("Instant.FieldTrial.Reason", 5, 10);
    return INACTIVE;
  }

  if (!prefs->GetBoolean(prefs::kSearchSuggestEnabled)) {
    UMA_HISTOGRAM_ENUMERATION("Instant.FieldTrial.Reason", 6, 10);
    return INACTIVE;
  }

  if (prefs->IsManagedPreference(prefs::kInstantEnabled)) {
    UMA_HISTOGRAM_ENUMERATION("Instant.FieldTrial.Reason", 7, 10);
    return INACTIVE;
  }

  UMA_HISTOGRAM_ENUMERATION("Instant.FieldTrial.Reason", 0, 10);

  if (group == g_instant)
    return INSTANT;
  if (group == g_suggest)
    return SUGGEST;
  if (group == g_hidden)
    return HIDDEN;
  if (group == g_silent)
    return SILENT;
  if (group == g_control)
    return CONTROL;

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
  bool uma = MetricsServiceHelper::IsMetricsReportingEnabled();
  switch (GetGroup(profile)) {
    case INACTIVE: return std::string();
    case INSTANT: if (uma) return "ix=ui&";
                           return "ix=ni&";
    case SUGGEST: if (uma) return "ix=ut&";
                           return "ix=nt&";
    case HIDDEN:  if (uma) return "ix=uh&";
                           return "ix=nh&";
    case SILENT:  if (uma) return "ix=us&";
                           return "ix=ns&";
    case CONTROL: if (uma) return "ix=uc&";
                           return "ix=nc&";
  }

  NOTREACHED();
  return std::string();
}

// static
bool InstantFieldTrial::ShouldSetSuggestedText(Profile* profile) {
  Group group = GetGroup(profile);
  return group != HIDDEN && group != SILENT;
}
