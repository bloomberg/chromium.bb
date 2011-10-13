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
int g_instant_control_a = 0;
int g_instant_control_b = 0;
int g_instant_experiment_a = 0;
int g_instant_experiment_b = 0;

int g_hidden_control_a = 0;
int g_hidden_control_b = 0;
int g_hidden_experiment_a = 0;
int g_hidden_experiment_b = 0;

}

// static
void InstantFieldTrial::Activate() {
  scoped_refptr<base::FieldTrial> trial(
      new base::FieldTrial("Instant", 1000, "Inactive", 2012, 7, 1));

  // Try to give the user a consistent experience, if possible.
  if (base::FieldTrialList::IsOneTimeRandomizationEnabled())
    trial->UseOneTimeRandomization();

  // Each group is of size 5%.
  g_instant_control_a = trial->AppendGroup("InstantControlA", 50);
  g_instant_control_b = trial->AppendGroup("InstantControlB", 50);
  g_instant_experiment_a = trial->AppendGroup("InstantExperimentA", 50);
  g_instant_experiment_b = trial->AppendGroup("InstantExperimentB", 50);

  g_hidden_control_a = trial->AppendGroup("HiddenControlA", 50);
  g_hidden_control_b = trial->AppendGroup("HiddenControlB", 50);
  g_hidden_experiment_a = trial->AppendGroup("HiddenExperimentA", 50);
  g_hidden_experiment_b = trial->AppendGroup("HiddenExperimentB", 50);
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
    else
      return INACTIVE;
  }

  const int group = base::FieldTrialList::FindValue("Instant");
  if (group == base::FieldTrial::kNotFinalized ||
      group == base::FieldTrial::kDefaultGroupNumber) {
    return INACTIVE;
  }

  if (!profile)
    return INACTIVE;

  const PrefService* prefs = profile->GetPrefs();
  if (!prefs ||
      prefs->GetBoolean(prefs::kInstantEnabledOnce) ||
      prefs->IsManagedPreference(prefs::kInstantEnabled)) {
    return INACTIVE;
  }

  // HIDDEN groups.
  if (group == g_hidden_control_a)
    return HIDDEN_CONTROL_A;
  if (group == g_hidden_control_b)
    return HIDDEN_CONTROL_B;
  if (group == g_hidden_experiment_a)
    return HIDDEN_EXPERIMENT_A;
  if (group == g_hidden_experiment_b)
    return HIDDEN_EXPERIMENT_B;

  // INSTANT group users must meet some extra requirements.
  if (profile->IsOffTheRecord() ||
      !MetricsServiceHelper::IsMetricsReportingEnabled() ||
      !prefs->GetBoolean(prefs::kSearchSuggestEnabled)) {
    return INACTIVE;
  }

  // INSTANT groups.
  if (group == g_instant_control_a)
    return INSTANT_CONTROL_A;
  if (group == g_instant_control_b)
    return INSTANT_CONTROL_B;
  if (group == g_instant_experiment_a)
    return INSTANT_EXPERIMENT_A;
  if (group == g_instant_experiment_b)
    return INSTANT_EXPERIMENT_B;

  NOTREACHED();
  return INACTIVE;
}

// static
bool InstantFieldTrial::IsExperimentGroup(Profile* profile) {
  Group group = GetGroup(profile);
  return group == INSTANT_EXPERIMENT_A || group == INSTANT_EXPERIMENT_B ||
         group == HIDDEN_EXPERIMENT_A || group == HIDDEN_EXPERIMENT_B;
}

// static
bool InstantFieldTrial::IsInstantExperiment(Profile* profile) {
  Group group = GetGroup(profile);
  return group == INSTANT_EXPERIMENT_A || group == INSTANT_EXPERIMENT_B;
}

// static
bool InstantFieldTrial::IsHiddenExperiment(Profile* profile) {
  Group group = GetGroup(profile);
  return group == HIDDEN_EXPERIMENT_A || group == HIDDEN_EXPERIMENT_B;
}

// static
std::string InstantFieldTrial::GetGroupName(Profile* profile) {
  switch (GetGroup(profile)) {
    case INACTIVE: return std::string();

    case INSTANT_CONTROL_A: return "_InstantControlA";
    case INSTANT_CONTROL_B: return "_InstantControlB";
    case INSTANT_EXPERIMENT_A: return "_InstantExperimentA";
    case INSTANT_EXPERIMENT_B: return "_InstantExperimentB";

    case HIDDEN_CONTROL_A: return "_HiddenControlA";
    case HIDDEN_CONTROL_B: return "_HiddenControlB";
    case HIDDEN_EXPERIMENT_A: return "_HiddenExperimentA";
    case HIDDEN_EXPERIMENT_B: return "_HiddenExperimentB";
  }

  NOTREACHED();
  return std::string();
}

// static
std::string InstantFieldTrial::GetGroupAsUrlParam(Profile* profile) {
  switch (GetGroup(profile)) {
    case INACTIVE: return std::string();

    case INSTANT_CONTROL_A: return "ix=ica&";
    case INSTANT_CONTROL_B: return "ix=icb&";
    case INSTANT_EXPERIMENT_A: return "ix=iea&";
    case INSTANT_EXPERIMENT_B: return "ix=ieb&";

    case HIDDEN_CONTROL_A: return "ix=hca&";
    case HIDDEN_CONTROL_B: return "ix=hcb&";
    case HIDDEN_EXPERIMENT_A: return "ix=hea&";
    case HIDDEN_EXPERIMENT_B: return "ix=heb&";
  }

  NOTREACHED();
  return std::string();
}
