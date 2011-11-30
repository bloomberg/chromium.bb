// Copyright (c) 2011 The Chromium Authors. All rights reserved.
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
int g_instant_experiment_a = 0;
int g_instant_experiment_b = 0;

int g_hidden_experiment_a = 0;
int g_hidden_experiment_b = 0;

int g_silent_experiment_a = 0;
int g_silent_experiment_b = 0;

int g_suggest_experiment_a = 0;
int g_suggest_experiment_b = 0;

int g_uma_control_a = 0;
int g_uma_control_b = 0;

int g_all_control_a = 0;
int g_all_control_b = 0;

}

// static
void InstantFieldTrial::Activate() {
  scoped_refptr<base::FieldTrial> trial(
      new base::FieldTrial("Instant", 1000, "Inactive", 2012, 7, 1));

  // Try to give the user a consistent experience, if possible.
  if (base::FieldTrialList::IsOneTimeRandomizationEnabled())
    trial->UseOneTimeRandomization();

  g_instant_experiment_a = trial->AppendGroup("InstantExperimentA", 50);
  g_instant_experiment_b = trial->AppendGroup("InstantExperimentB", 50);

  g_hidden_experiment_a = trial->AppendGroup("HiddenExperimentA", 50);
  g_hidden_experiment_b = trial->AppendGroup("HiddenExperimentB", 50);

  g_silent_experiment_a = trial->AppendGroup("SilentExperimentA", 340);
  g_silent_experiment_b = trial->AppendGroup("SilentExperimentB", 340);

  g_suggest_experiment_a = trial->AppendGroup("SuggestExperimentA", 50);
  g_suggest_experiment_b = trial->AppendGroup("SuggestExperimentB", 50);

  g_uma_control_a = trial->AppendGroup("UmaControlA", 5);
  g_uma_control_b = trial->AppendGroup("UmaControlB", 5);

  g_all_control_a = trial->AppendGroup("AllControlA", 5);
  g_all_control_b = trial->AppendGroup("AllControlB", 5);
}

// static
InstantFieldTrial::Group InstantFieldTrial::GetGroup(Profile* profile) {
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kInstantFieldTrial)) {
    std::string switch_value =
        command_line->GetSwitchValueASCII(switches::kInstantFieldTrial);
    if (switch_value == switches::kInstantFieldTrialInstant)
      return INSTANT_EXPERIMENT_A;
    else if (switch_value == switches::kInstantFieldTrialHidden)
      return HIDDEN_EXPERIMENT_A;
    else if (switch_value == switches::kInstantFieldTrialSilent)
      return SILENT_EXPERIMENT_A;
    else if (switch_value == switches::kInstantFieldTrialSuggest)
      return SUGGEST_EXPERIMENT_A;
    else
      return INACTIVE;
  }

  const int group = base::FieldTrialList::FindValue("Instant");
  if (group == base::FieldTrial::kNotFinalized ||
      group == base::FieldTrial::kDefaultGroupNumber) {
    return INACTIVE;
  }

  const PrefService* prefs = profile ? profile->GetPrefs() : NULL;
  if (!prefs ||
      prefs->GetBoolean(prefs::kInstantEnabledOnce) ||
      prefs->IsManagedPreference(prefs::kInstantEnabled)) {
    return INACTIVE;
  }

  // First, deal with the groups that don't require UMA opt-in.
  if (group == g_silent_experiment_a)
    return SILENT_EXPERIMENT_A;
  if (group == g_silent_experiment_b)
    return SILENT_EXPERIMENT_B;

  if (group == g_all_control_a)
    return ALL_CONTROL_A;
  if (group == g_all_control_b)
    return ALL_CONTROL_B;

  // All other groups require UMA and suggest, else bounce back to INACTIVE.
  if (profile->IsOffTheRecord() ||
      !MetricsServiceHelper::IsMetricsReportingEnabled() ||
      !prefs->GetBoolean(prefs::kSearchSuggestEnabled)) {
    return INACTIVE;
  }

  if (group == g_instant_experiment_a)
    return INSTANT_EXPERIMENT_A;
  if (group == g_instant_experiment_b)
    return INSTANT_EXPERIMENT_B;

  if (group == g_hidden_experiment_a)
    return HIDDEN_EXPERIMENT_A;
  if (group == g_hidden_experiment_b)
    return HIDDEN_EXPERIMENT_B;

  if (group == g_suggest_experiment_a)
    return SUGGEST_EXPERIMENT_A;
  if (group == g_suggest_experiment_b)
    return SUGGEST_EXPERIMENT_B;

  if (group == g_uma_control_a)
    return UMA_CONTROL_A;
  if (group == g_uma_control_b)
    return UMA_CONTROL_B;

  NOTREACHED();
  return INACTIVE;
}

// static
bool InstantFieldTrial::IsInstantExperiment(Profile* profile) {
  Group group = GetGroup(profile);
  return group == INSTANT_EXPERIMENT_A || group == INSTANT_EXPERIMENT_B ||
         group == HIDDEN_EXPERIMENT_A || group == HIDDEN_EXPERIMENT_B ||
         group == SILENT_EXPERIMENT_A || group == SILENT_EXPERIMENT_B ||
         group == SUGGEST_EXPERIMENT_A || group == SUGGEST_EXPERIMENT_B;
}

// static
bool InstantFieldTrial::IsHiddenExperiment(Profile* profile) {
  Group group = GetGroup(profile);
  return group == HIDDEN_EXPERIMENT_A || group == HIDDEN_EXPERIMENT_B ||
         group == SILENT_EXPERIMENT_A || group == SILENT_EXPERIMENT_B ||
         group == SUGGEST_EXPERIMENT_A || group == SUGGEST_EXPERIMENT_B;
}

// static
bool InstantFieldTrial::IsSilentExperiment(Profile* profile) {
  Group group = GetGroup(profile);
  return group == SILENT_EXPERIMENT_A || group == SILENT_EXPERIMENT_B;
}

// static
std::string InstantFieldTrial::GetGroupName(Profile* profile) {
  switch (GetGroup(profile)) {
    case INACTIVE: return std::string();

    case INSTANT_EXPERIMENT_A: return "_InstantExperimentA";
    case INSTANT_EXPERIMENT_B: return "_InstantExperimentB";

    case HIDDEN_EXPERIMENT_A: return "_HiddenExperimentA";
    case HIDDEN_EXPERIMENT_B: return "_HiddenExperimentB";

    case SILENT_EXPERIMENT_A: return "_SilentExperimentA";
    case SILENT_EXPERIMENT_B: return "_SilentExperimentB";

    case SUGGEST_EXPERIMENT_A: return "_SuggestExperimentA";
    case SUGGEST_EXPERIMENT_B: return "_SuggestExperimentB";

    case UMA_CONTROL_A: return "_UmaControlA";
    case UMA_CONTROL_B: return "_UmaControlB";

    case ALL_CONTROL_A: return "_AllControlA";
    case ALL_CONTROL_B: return "_AllControlB";
  }

  NOTREACHED();
  return std::string();
}

// static
std::string InstantFieldTrial::GetGroupAsUrlParam(Profile* profile) {
  switch (GetGroup(profile)) {
    case INACTIVE: return std::string();

    case INSTANT_EXPERIMENT_A: return "ix=iea&";
    case INSTANT_EXPERIMENT_B: return "ix=ieb&";

    case HIDDEN_EXPERIMENT_A: return "ix=hea&";
    case HIDDEN_EXPERIMENT_B: return "ix=heb&";

    case SILENT_EXPERIMENT_A: return "ix=sea&";
    case SILENT_EXPERIMENT_B: return "ix=seb&";

    case SUGGEST_EXPERIMENT_A: return "ix=tea&";
    case SUGGEST_EXPERIMENT_B: return "ix=teb&";

    case UMA_CONTROL_A: return "ix=uca&";
    case UMA_CONTROL_B: return "ix=ucb&";

    case ALL_CONTROL_A: return "ix=aca&";
    case ALL_CONTROL_B: return "ix=acb&";
  }

  NOTREACHED();
  return std::string();
}

// static
bool InstantFieldTrial::ShouldSetSuggestedText(Profile* profile) {
  Group group = GetGroup(profile);
  return !(group == HIDDEN_EXPERIMENT_A || group == HIDDEN_EXPERIMENT_B ||
           group == SILENT_EXPERIMENT_A || group == SILENT_EXPERIMENT_B);
}
