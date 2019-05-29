// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/search/local_ntp_first_run_field_trial_handler.h"

#include "base/feature_list.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/search/local_ntp_first_run_field_trials.h"
#include "chrome/browser/search/ntp_features.h"
#include "chrome/common/channel_info.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/version_info/version_info.h"

namespace {

// Probabilities for all field trial groups add up to kTotalProbability.
const base::FieldTrial::Probability kTotalProbability = 100;

const char kTrialName[] = "HideShortcutsOnNtp";
// Each field trial has 3 groups.
// HideShortcutsExperiment - see the NTP with shortcuts hidden.
// HideShortcutsControl - see shortcuts on the NTP; same size as the
//   HideShortcutsExperiment group.
// Default - see shortcuts on the NTP; are not part of the experiment.
const char kExperimentGroup[] = "HideShortcutsExperiment";
const char kControlGroup[] = "HideShortcutsControl";
const char kDefaultGroup[] = "Default";

void LoadConfigByChannel(const version_info::Channel& channel,
                         variations::VariationID* experiment_id,
                         variations::VariationID* control_id,
                         base::FieldTrial::Probability* experiment_size) {
  switch (channel) {
    // Shortcuts are hidden for 100% of new users on the Canary / Dev Channels.
    case version_info::Channel::CANARY:
    case version_info::Channel::DEV:
    case version_info::Channel::UNKNOWN:
      *experiment_id = 3315271;
      *control_id = 3315272;
      *experiment_size = 50;
      break;
    // Shortcuts are hidden for 50% of new users on the Beta Channel.
    case version_info::Channel::BETA:
      *experiment_id = 3315273;
      *control_id = 3315274;
      *experiment_size = 50;
      break;
    // Shortcuts are hidden for 1% of new users on the Stable Channel.
    case version_info::Channel::STABLE:
      *experiment_id = 3315275;
      *control_id = 3315276;
      *experiment_size = 1;
      break;
  }
}

}  // namespace

namespace ntp_first_run {

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  registry->RegisterBooleanPref(prefs::kNtpActivateHideShortcutsFieldTrial,
                                false);
}

void ActivateHideShortcutsOnNtpFieldTrial(base::FeatureList* feature_list,
                                          PrefService* local_state) {
  // The field trial is activated for fresh installs, or for clients that were
  // previously activated (during a fresh install). For all other existing
  // clients, the trial is not activated, and the experiment is unavailable.
  if (!first_run::IsChromeFirstRun() &&
      !local_state->GetBoolean(prefs::kNtpActivateHideShortcutsFieldTrial)) {
    return;
  }
  // Field trials do not take effect until configurations have been downloaded.
  // In order for field trials involving substantial UI changes to take effect
  // at first run, the configuration must be pre-defined in client code.
  variations::VariationID experiment_id;
  variations::VariationID control_id;
  base::FieldTrial::Probability experiment_size;
  LoadConfigByChannel(chrome::GetChannel(), &experiment_id, &control_id,
                      &experiment_size);

  LocalNtpFirstRunFieldTrialConfig config(kTrialName);
  config.AddGroup(kExperimentGroup, experiment_id, experiment_size);
  config.AddGroup(kControlGroup, control_id, experiment_size);

  // The field trial can only be loaded once per browser session
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial(
          config.trial_name(), kTotalProbability, kDefaultGroup,
          base::FieldTrial::ONE_TIME_RANDOMIZED,
          /*default_group_number=*/nullptr));
  for (const auto& group : config.groups()) {
    variations::AssociateGoogleVariationID(variations::GOOGLE_WEB_PROPERTIES,
                                           config.trial_name(), group.name(),
                                           group.variation());
    trial->AppendGroup(group.name(), group.percentage());
  }

  const std::string& chosen_group_name = trial->GetGroupNameWithoutActivation();
  base::FeatureList::OverrideState feature_state =
      (chosen_group_name == kExperimentGroup)
          ? base::FeatureList::OVERRIDE_ENABLE_FEATURE
          : base::FeatureList::OVERRIDE_DISABLE_FEATURE;

  feature_list->RegisterFieldTrialOverride(features::kHideShortcutsOnNtp.name,
                                           feature_state, trial.get());

  // This pref ensures the field trial can be loaded again for clients that
  // were activated in a previous browser session.
  local_state->SetBoolean(prefs::kNtpActivateHideShortcutsFieldTrial, true);
}

}  // namespace ntp_first_run
