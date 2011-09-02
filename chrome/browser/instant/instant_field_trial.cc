// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/instant/instant_field_trial.h"

#include "base/metrics/field_trial.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"

namespace {

// Field trial IDs of the control and experiment groups. Though they are not
// literally "const", they are set only once, in Activate() below.
int g_control_group_id_1 = 0;
int g_control_group_id_2 = 0;
int g_experiment_group_id_1 = 0;
int g_experiment_group_id_2 = 0;

}

// static
void InstantFieldTrial::Activate() {
  scoped_refptr<base::FieldTrial> trial(
      new base::FieldTrial("Instant", 1000, "InstantInactive", 2012, 1, 1));

  // One-time randomization is disabled if the user hasn't opted-in to UMA.
  if (!base::FieldTrialList::IsOneTimeRandomizationEnabled())
    return;
  trial->UseOneTimeRandomization();

  g_control_group_id_1 = trial->AppendGroup("InstantControl1", 495);
  g_control_group_id_2 = trial->AppendGroup("InstantControl2", 495);
  g_experiment_group_id_1 = trial->AppendGroup("InstantExperiment1", 5);
  g_experiment_group_id_2 = trial->AppendGroup("InstantExperiment2", 5);
}

// static
InstantFieldTrial::Group InstantFieldTrial::GetGroup(Profile* profile) {
  if (!profile || profile->IsOffTheRecord())
    return INACTIVE;

  const PrefService* prefs = profile->GetPrefs();
  if (!prefs ||
      !MetricsServiceHelper::IsMetricsReportingEnabled() ||
      !prefs->GetBoolean(prefs::kSearchSuggestEnabled) ||
      prefs->GetBoolean(prefs::kInstantEnabledOnce) ||
      prefs->IsManagedPreference(prefs::kInstantEnabled)) {
    return INACTIVE;
  }

  const int group = base::FieldTrialList::FindValue("Instant");

  if (group == base::FieldTrial::kNotFinalized ||
      group == base::FieldTrial::kDefaultGroupNumber) {
    return INACTIVE;
  }

  return group == g_control_group_id_1    ? CONTROL1 :
         group == g_control_group_id_2    ? CONTROL2 :
         group == g_experiment_group_id_1 ? EXPERIMENT1 :
         group == g_experiment_group_id_2 ? EXPERIMENT2
                                          : INACTIVE;
}

// static
bool InstantFieldTrial::IsExperimentGroup(Profile* profile) {
  Group group = GetGroup(profile);
  return group == EXPERIMENT1 || group == EXPERIMENT2;
}

// static
std::string InstantFieldTrial::GetGroupName(Profile* profile) {
  switch (GetGroup(profile)) {
    case INACTIVE:    return "InstantInactive";
    case CONTROL1:    return "InstantControl1";
    case CONTROL2:    return "InstantControl2";
    case EXPERIMENT1: return "InstantExperiment1";
    case EXPERIMENT2: return "InstantExperiment2";
  }
  NOTREACHED();
  return "InstantUnknown";
}

// static
std::string InstantFieldTrial::GetGroupAsUrlParam(Profile* profile) {
  switch (GetGroup(profile)) {
    case INACTIVE:    return std::string();
    case CONTROL1:    return "ix=c1&";
    case CONTROL2:    return "ix=c2&";
    case EXPERIMENT1: return "ix=e1&";
    case EXPERIMENT2: return "ix=e2&";
  }
  NOTREACHED();
  return std::string();
}
