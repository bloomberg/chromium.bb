// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/metrics/variations/uniformity_field_trials.h"

#include "base/metrics/field_trial.h"
#include "base/stringprintf.h"
#include "chrome/common/metrics/variations/variations_util.h"

namespace {

// Set up a uniformity field trial. |one_time_randomized| indicates if the
// field trial is one-time randomized or session-randomized. |trial_name_string|
// must contain a "%d" since the percentage of the group will be inserted in
// the trial name. |num_trial_groups| must be a divisor of 100 (e.g. 5, 20)
void SetupSingleUniformityFieldTrial(
    bool one_time_randomized,
    const std::string& trial_name_string,
    const chrome_variations::VariationID trial_base_id,
    int num_trial_groups) {
  // Probability per group remains constant for all uniformity trials, what
  // changes is the probability divisor.
  static const base::FieldTrial::Probability kProbabilityPerGroup = 1;
  const std::string kDefaultGroupName = "default";
  const base::FieldTrial::Probability divisor = num_trial_groups;

  DCHECK_EQ(100 % num_trial_groups, 0);
  const int group_percent = 100 / num_trial_groups;
  const std::string trial_name = StringPrintf(trial_name_string.c_str(),
                                              group_percent);

  DVLOG(1) << "Trial name = " << trial_name;

  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial(
          trial_name, divisor, kDefaultGroupName, 2015, 1, 1, NULL));
  if (one_time_randomized)
    trial->UseOneTimeRandomization();
  chrome_variations::AssociateGoogleVariationID(
      chrome_variations::GOOGLE_WEB_PROPERTIES, trial_name, kDefaultGroupName,
      trial_base_id);
  chrome_variations::AssociateGoogleVariationID(
      chrome_variations::GOOGLE_UPDATE_SERVICE, trial_name, kDefaultGroupName,
      trial_base_id);

  // Loop starts with group 1 because the field trial automatically creates a
  // default group, which would be group 0.
  for (int group_number = 1; group_number < num_trial_groups; ++group_number) {
    const std::string group_name = StringPrintf("group_%02d", group_number);
    DVLOG(1) << "    Group name = " << group_name;
    trial->AppendGroup(group_name, kProbabilityPerGroup);
    chrome_variations::AssociateGoogleVariationID(
        chrome_variations::GOOGLE_WEB_PROPERTIES, trial_name, group_name,
        static_cast<chrome_variations::VariationID>(trial_base_id +
                                                    group_number));
    chrome_variations::AssociateGoogleVariationID(
        chrome_variations::GOOGLE_UPDATE_SERVICE, trial_name, group_name,
        static_cast<chrome_variations::VariationID>(trial_base_id +
                                                    group_number));
  }

  // Now that all groups have been appended, call group() on the trial to
  // ensure that our trial is registered. This resolves an off-by-one issue
  // where the default group never gets chosen if we don't "use" the trial.
  const int chosen_group = trial->group();
  DVLOG(1) << "Chosen Group: " << chosen_group;
}

// Setup a 50% uniformity trial for new installs only. This is accomplished by
// disabling the trial on clients that were installed before a specified date.
void SetupNewInstallUniformityTrial(const base::Time& install_date) {
  const base::Time::Exploded kStartDate = {
    2012, 11, 0, 6,  // Nov 6, 2012
    0, 0, 0, 0       // 00:00:00.000
  };
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial(
          "UMA-New-Install-Uniformity-Trial", 100, "Disabled",
          2015, 1, 1, NULL));
  trial->UseOneTimeRandomization();
  trial->AppendGroup("Control", 50);
  trial->AppendGroup("Experiment", 50);
  const base::Time start_date = base::Time::FromLocalExploded(kStartDate);
  if (install_date < start_date)
    trial->Disable();
  else
    trial->group();
}

}  // namespace

namespace chrome_variations {

void SetupUniformityFieldTrials(const base::Time& install_date) {
  // One field trial will be created for each entry in this array. The i'th
  // field trial will have |trial_sizes[i]| groups in it, including the default
  // group. Each group will have a probability of 1/|trial_sizes[i]|.
  const int num_trial_groups[] = { 100, 20, 10, 5, 2 };

  // Declare our variation ID bases along side this array so we can loop over it
  // and assign the IDs appropriately. So for example, the 1 percent experiments
  // should have a size of 100 (100/100 = 1).
  const chrome_variations::VariationID trial_base_ids[] = {
    chrome_variations::UNIFORMITY_1_PERCENT_BASE,
    chrome_variations::UNIFORMITY_5_PERCENT_BASE,
    chrome_variations::UNIFORMITY_10_PERCENT_BASE,
    chrome_variations::UNIFORMITY_20_PERCENT_BASE,
    chrome_variations::UNIFORMITY_50_PERCENT_BASE
  };

  const std::string kOneTimeRandomizedTrialName =
      "UMA-Uniformity-Trial-%d-Percent";
  for (size_t i = 0; i < arraysize(num_trial_groups); ++i) {
    SetupSingleUniformityFieldTrial(true, kOneTimeRandomizedTrialName,
                                    trial_base_ids[i], num_trial_groups[i]);
  }

  // Setup a 5% session-randomized uniformity trial.
  const std::string kSessionRandomizedTrialName =
      "UMA-Session-Randomized-Uniformity-Trial-%d-Percent";
  SetupSingleUniformityFieldTrial(false, kSessionRandomizedTrialName,
      chrome_variations::UNIFORMITY_SESSION_RANDOMIZED_5_PERCENT_BASE, 20);

  SetupNewInstallUniformityTrial(install_date);
}

}  // namespace chrome_variations
