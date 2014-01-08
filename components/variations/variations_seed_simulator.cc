// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_seed_simulator.h"

#include <map>

#include "base/metrics/field_trial.h"
#include "components/variations/processed_study.h"
#include "components/variations/proto/study.pb.h"
#include "components/variations/variations_associated_data.h"

namespace chrome_variations {

namespace {

// Fills in |current_state| with the current process' active field trials, as a
// map of trial names to group names.
void GetCurrentTrialState(std::map<std::string, std::string>* current_state) {
  base::FieldTrial::ActiveGroups trial_groups;
  base::FieldTrialList::GetActiveFieldTrialGroups(&trial_groups);
  for (size_t i = 0; i < trial_groups.size(); ++i)
    (*current_state)[trial_groups[i].trial_name] = trial_groups[i].group_name;
}

// Simulate group assignment for the specified study with PERMANENT consistency.
// Returns the experiment group that will be selected. Mirrors logic in
// VariationsSeedProcessor::CreateTrialFromStudy().
std::string SimulateGroupAssignment(
    const base::FieldTrial::EntropyProvider& entropy_provider,
    const ProcessedStudy& processed_study) {
  const Study& study = *processed_study.study();
  DCHECK_EQ(Study_Consistency_PERMANENT, study.consistency());

  const double entropy_value =
      entropy_provider.GetEntropyForTrial(study.name(),
                                          study.randomization_seed());
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrial::CreateSimulatedFieldTrial(
          study.name(), processed_study.total_probability(),
          study.default_experiment_name(), entropy_value));

  for (int i = 0; i < study.experiment_size(); ++i) {
    const Study_Experiment& experiment = study.experiment(i);
    // TODO(asvitkine): This needs to properly handle the case where a group was
    // forced via forcing_flag in the current state, so that it is not treated
    // as changed.
    if (!experiment.has_forcing_flag() &&
        experiment.name() != study.default_experiment_name()) {
      trial->AppendGroup(experiment.name(), experiment.probability_weight());
    }
  }
  if (processed_study.is_expired())
    trial->Disable();
  return trial->group_name();
}

// Finds an experiment in |study| with name |experiment_name| and returns it,
// or NULL if it does not exist.
const Study_Experiment* FindExperiment(const Study& study,
                                       const std::string& experiment_name) {
  for (int i = 0; i < study.experiment_size(); ++i) {
    if (study.experiment(i).name() == experiment_name)
      return &study.experiment(i);
  }
  return NULL;
}

// Checks whether experiment params set for |experiment| on |study| are exactly
// equal to the params registered for the corresponding field trial in the
// current process.
bool VariationParamsAreEqual(const Study& study,
                             const Study_Experiment& experiment) {
  std::map<std::string, std::string> params;
  GetVariationParams(study.name(), &params);

  if (static_cast<int>(params.size()) != experiment.param_size())
    return false;

  for (int i = 0; i < experiment.param_size(); ++i) {
    std::map<std::string, std::string>::const_iterator it =
        params.find(experiment.param(i).name());
    if (it == params.end() || it->second != experiment.param(i).value())
      return false;
  }

  return true;
}

}  // namespace

VariationsSeedSimulator::VariationsSeedSimulator(
    const base::FieldTrial::EntropyProvider& entropy_provider)
    : entropy_provider_(entropy_provider) {
}

VariationsSeedSimulator::~VariationsSeedSimulator() {
}

int VariationsSeedSimulator::ComputeDifferences(
    const std::vector<ProcessedStudy>& processed_studies) {
  std::map<std::string, std::string> current_state;
  GetCurrentTrialState(&current_state);
  int group_change_count = 0;

  for (size_t i = 0; i < processed_studies.size(); ++i) {
    const Study& study = *processed_studies[i].study();
    std::map<std::string, std::string>::const_iterator it =
        current_state.find(study.name());

    // Skip studies that aren't activated in the current state.
    // TODO(asvitkine): This should be handled more intelligently. There are
    // several cases that fall into this category:
    //   1) There's an existing field trial with this name but it is not active.
    //   2) There's an existing expired field trial with this name, which is
    //      also not considered as active.
    //   3) This is a new study config that previously didn't exist.
    // The above cases should be differentiated and handled explicitly.
    if (it == current_state.end())
      continue;

    // Study exists in the current state, check whether its group will change.
    // Note: The logic below does the right thing if study consistency changes,
    // as it doesn't rely on the previous study consistency.
    const std::string& selected_group = it->second;
    if (study.consistency() == Study_Consistency_PERMANENT) {
      if (PermanentStudyGroupChanged(processed_studies[i], selected_group))
        ++group_change_count;
    } else if (study.consistency() == Study_Consistency_SESSION) {
      if (SessionStudyGroupChanged(processed_studies[i], selected_group))
        ++group_change_count;
    }
  }

  // TODO(asvitkine): Handle removed studies (i.e. studies that existed in the
  // old seed, but were removed). This will require tracking the set of studies
  // that were created from the original seed.

  return group_change_count;
}

bool VariationsSeedSimulator::PermanentStudyGroupChanged(
    const ProcessedStudy& processed_study,
    const std::string& selected_group) {
  const Study& study = *processed_study.study();
  DCHECK_EQ(Study_Consistency_PERMANENT, study.consistency());

  const std::string simulated_group = SimulateGroupAssignment(entropy_provider_,
                                                              processed_study);
  // TODO(asvitkine): Sometimes group names are changed without changing any
  // behavior (e.g. if the behavior is controlled entirely via params). Support
  // a mechanism to bypass this check.
  if (simulated_group != selected_group)
    return true;

  const Study_Experiment* experiment = FindExperiment(study, selected_group);
  DCHECK(experiment);
  return !VariationParamsAreEqual(study, *experiment);
}

bool VariationsSeedSimulator::SessionStudyGroupChanged(
    const ProcessedStudy& processed_study,
    const std::string& selected_group) {
  const Study& study = *processed_study.study();
  DCHECK_EQ(Study_Consistency_SESSION, study.consistency());

  if (processed_study.is_expired() &&
      selected_group != study.default_experiment_name()) {
    // An expired study will result in the default group being selected - mark
    // it as changed if the current group differs from the default.
    return true;
  }

  const Study_Experiment* experiment = FindExperiment(study, selected_group);
  if (!experiment)
    return true;
  if (experiment->probability_weight() == 0 &&
      !experiment->has_forcing_flag()) {
    return true;
  }

  // Current group exists in the study - check whether its params changed.
  return !VariationParamsAreEqual(study, *experiment);
}

}  // namespace chrome_variations
