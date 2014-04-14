// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/variations/variations_seed_processor.h"

#include <vector>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "components/variations/processed_study.h"
#include "components/variations/study_filtering.h"
#include "components/variations/variations_associated_data.h"

namespace chrome_variations {

namespace {

// Associates the variations params of |experiment|, if present.
void RegisterExperimentParams(const Study& study,
                              const Study_Experiment& experiment) {
  std::map<std::string, std::string> params;
  for (int i = 0; i < experiment.param_size(); ++i) {
    if (experiment.param(i).has_name() && experiment.param(i).has_value())
      params[experiment.param(i).name()] = experiment.param(i).value();
  }
  if (!params.empty())
    AssociateVariationParams(study.name(), experiment.name(), params);
}

// If there are variation ids associated with |experiment|, register the
// variation ids.
void RegisterVariationIds(const Study_Experiment& experiment,
                          const std::string& trial_name) {
  if (experiment.has_google_web_experiment_id()) {
    const VariationID variation_id =
        static_cast<VariationID>(experiment.google_web_experiment_id());
    AssociateGoogleVariationIDForce(GOOGLE_WEB_PROPERTIES,
                                    trial_name,
                                    experiment.name(),
                                    variation_id);
  }
  if (experiment.has_google_update_experiment_id()) {
    const VariationID variation_id =
        static_cast<VariationID>(experiment.google_update_experiment_id());
    AssociateGoogleVariationIDForce(GOOGLE_UPDATE_SERVICE,
                                    trial_name,
                                    experiment.name(),
                                    variation_id);
  }
}

}  // namespace

VariationsSeedProcessor::VariationsSeedProcessor() {
}

VariationsSeedProcessor::~VariationsSeedProcessor() {
}

void VariationsSeedProcessor::CreateTrialsFromSeed(
    const VariationsSeed& seed,
    const std::string& locale,
    const base::Time& reference_date,
    const base::Version& version,
    Study_Channel channel,
    Study_FormFactor form_factor) {
  std::vector<ProcessedStudy> filtered_studies;
  FilterAndValidateStudies(seed, locale, reference_date, version, channel,
                           form_factor, &filtered_studies);

  for (size_t i = 0; i < filtered_studies.size(); ++i)
    CreateTrialFromStudy(filtered_studies[i]);
}

void VariationsSeedProcessor::CreateTrialFromStudy(
    const ProcessedStudy& processed_study) {
  const Study& study = *processed_study.study();

  // Check if any experiments need to be forced due to a command line
  // flag. Force the first experiment with an existing flag.
  CommandLine* command_line = CommandLine::ForCurrentProcess();
  for (int i = 0; i < study.experiment_size(); ++i) {
    const Study_Experiment& experiment = study.experiment(i);
    if (experiment.has_forcing_flag() &&
        command_line->HasSwitch(experiment.forcing_flag())) {
      scoped_refptr<base::FieldTrial> trial(
          base::FieldTrialList::CreateFieldTrial(study.name(),
                                                 experiment.name()));
      RegisterExperimentParams(study, experiment);
      RegisterVariationIds(experiment, study.name());
      if (study.activation_type() == Study_ActivationType_ACTIVATION_AUTO)
        trial->group();

      DVLOG(1) << "Trial " << study.name() << " forced by flag: "
               << experiment.forcing_flag();
      return;
    }
  }

  uint32 randomization_seed = 0;
  base::FieldTrial::RandomizationType randomization_type =
      base::FieldTrial::SESSION_RANDOMIZED;
  if (study.has_consistency() &&
      study.consistency() == Study_Consistency_PERMANENT) {
    randomization_type = base::FieldTrial::ONE_TIME_RANDOMIZED;
    if (study.has_randomization_seed())
      randomization_seed = study.randomization_seed();
  }

  // The trial is created without specifying an expiration date because the
  // expiration check in field_trial.cc is based on the build date. Instead,
  // the expiration check using |reference_date| is done explicitly below.
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrialWithRandomizationSeed(
          study.name(), processed_study.total_probability(),
          study.default_experiment_name(),
          base::FieldTrialList::kNoExpirationYear, 1, 1, randomization_type,
          randomization_seed, NULL));

  for (int i = 0; i < study.experiment_size(); ++i) {
    const Study_Experiment& experiment = study.experiment(i);
    RegisterExperimentParams(study, experiment);

    // Groups with forcing flags have probability 0 and will never be selected.
    // Therefore, there's no need to add them to the field trial.
    if (experiment.has_forcing_flag())
      continue;

    if (experiment.name() != study.default_experiment_name())
      trial->AppendGroup(experiment.name(), experiment.probability_weight());

    RegisterVariationIds(experiment, study.name());
  }

  trial->SetForced();
  if (processed_study.is_expired())
    trial->Disable();
  else if (study.activation_type() == Study_ActivationType_ACTIVATION_AUTO)
    trial->group();
}

}  // namespace chrome_variations
