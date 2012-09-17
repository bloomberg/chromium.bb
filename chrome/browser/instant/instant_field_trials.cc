// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/instant/instant_field_trials.h"

#include "base/metrics/field_trial.h"
#include "chrome/common/metrics/variations/variation_ids.h"
#include "chrome/common/metrics/variations/variations_util.h"

namespace instant {

void SetupInstantFieldTrials() {
  const base::FieldTrial::Probability num_buckets = 1000;
  // Default to 5%.  This number may be overridden by the Variations server.
  const base::FieldTrial::Probability group_probability = 50;
  const std::string trial_name = "InstantDummy";
  const std::string default_group_name = "DefaultGroup";
  const std::string control_group_name = "Control";
  const std::string experiment_one_group_name = "Experiment1";
  const std::string experiment_two_group_name = "Experiment2";
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial(
          trial_name, num_buckets, default_group_name,
          2013, 6, 30, NULL));
  // Give users a consistent experience.
  if (base::FieldTrialList::IsOneTimeRandomizationEnabled())
    trial->UseOneTimeRandomization();

  // Create field trial groups.
  trial->AppendGroup(control_group_name, group_probability);
  trial->AppendGroup(experiment_one_group_name, group_probability);
  trial->AppendGroup(experiment_two_group_name, group_probability);

  // Setup Google Variation IDs for each group in the field trial so Google
  // servers are aware of the variants.
  chrome_variations::AssociateGoogleVariationID(trial_name, default_group_name,
      chrome_variations::kDummyInstantIDDefault);
  chrome_variations::AssociateGoogleVariationID(trial_name, control_group_name,
      chrome_variations::kDummyInstantIDControl);
  chrome_variations::AssociateGoogleVariationID(trial_name,
      experiment_one_group_name,
      chrome_variations::kDummyInstantIDExperimentOne);
  chrome_variations::AssociateGoogleVariationID(trial_name,
      experiment_two_group_name,
      chrome_variations::kDummyInstantIDExperimentTwo);
}

}  // namespace instant
