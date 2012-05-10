// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_field_trial.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/browser/predictors/autocomplete_action_predictor.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"

namespace prerender {

namespace {

const char kOmniboxTrialName[] = "PrerenderFromOmnibox";
int g_omnibox_trial_default_group_number = -1;

void SetupPrefetchFieldTrial() {
  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  if (channel == chrome::VersionInfo::CHANNEL_STABLE ||
      channel == chrome::VersionInfo::CHANNEL_BETA) {
    return;
  }

  const base::FieldTrial::Probability divisor = 1000;
  const base::FieldTrial::Probability prefetch_probability = 500;
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial(
          "Prefetch", divisor, "ContentPrefetchPrefetchOff",
          2013, 6, 30, NULL));
  const int kPrefetchOnGroup = trial->AppendGroup("ContentPrefetchPrefetchOn",
                                                  prefetch_probability);
  PrerenderManager::SetIsPrefetchEnabled(trial->group() == kPrefetchOnGroup);
}

void SetupPrerenderFieldTrial() {
  base::FieldTrial::Probability divisor = 1000;

  base::FieldTrial::Probability exp1_probability = 166;
  base::FieldTrial::Probability exp1_5min_ttl_probability = 83;
  base::FieldTrial::Probability control1_probability = 166;
  base::FieldTrial::Probability no_use1_probability = 83;

  base::FieldTrial::Probability exp2_probability = 167;
  base::FieldTrial::Probability exp2_5min_ttl_probability = 84;
  base::FieldTrial::Probability control2_probability = 167;
  base::FieldTrial::Probability no_use2_probability = 84;

  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  if (channel == chrome::VersionInfo::CHANNEL_STABLE ||
      channel == chrome::VersionInfo::CHANNEL_BETA) {
    exp1_probability = 490;
    exp1_5min_ttl_probability = 5;
    control1_probability = 5;
    no_use1_probability = 0;
    exp2_probability = 490;
    exp2_5min_ttl_probability = 5;
    control2_probability = 5;
    no_use2_probability = 0;
  }
  CHECK_EQ(divisor, exp1_probability + exp1_5min_ttl_probability +
           control1_probability + no_use1_probability + exp2_probability +
           exp2_5min_ttl_probability + control2_probability +
           no_use2_probability);
  int experiment_1_group = -1;
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial(
          "Prerender", divisor, "ContentPrefetchPrerender1",
          2013, 6, 30, &experiment_1_group));

  const int experiment_15_min_TTL_group =
      trial->AppendGroup("ContentPrefetchPrerenderExp5minTTL1",
                         exp1_5min_ttl_probability);
  const int control_1_group =
      trial->AppendGroup("ContentPrefetchPrerenderControl1",
                         control1_probability);
  const int no_use_1_group =
      trial->AppendGroup("ContentPrefetchPrerenderNoUse1",
                         no_use1_probability);
  const int experiment_2_group =
      trial->AppendGroup("ContentPrefetchPrerender2",
                         exp2_probability);
  const int experiment_25_min_TTL_group =
      trial->AppendGroup("ContentPrefetchPrerenderExp5minTTL2",
                         exp2_5min_ttl_probability);
  const int control_2_group =
      trial->AppendGroup("ContentPrefetchPrerenderControl2",
                         control2_probability);
  const int no_use_2_group =
      trial->AppendGroup("ContentPrefetchPrerenderNoUse2",
                         no_use2_probability);
  const int trial_group = trial->group();
  if (trial_group == experiment_1_group ||
      trial_group == experiment_2_group) {
    PrerenderManager::SetMode(
        PrerenderManager::PRERENDER_MODE_EXPERIMENT_PRERENDER_GROUP);
  } else if (trial_group == experiment_15_min_TTL_group ||
             trial_group == experiment_25_min_TTL_group) {
    PrerenderManager::SetMode(
        PrerenderManager::PRERENDER_MODE_EXPERIMENT_5MIN_TTL_GROUP);
  } else if (trial_group == control_1_group ||
             trial_group == control_2_group) {
    PrerenderManager::SetMode(
        PrerenderManager::PRERENDER_MODE_EXPERIMENT_CONTROL_GROUP);
  } else if (trial_group == no_use_1_group ||
             trial_group == no_use_2_group) {
    PrerenderManager::SetMode(
        PrerenderManager::PRERENDER_MODE_EXPERIMENT_NO_USE_GROUP);
  } else {
    NOTREACHED();
  }
}

}  // end namespace

void ConfigureOmniboxPrerender();

void ConfigurePrefetchAndPrerender(const CommandLine& command_line) {
  enum PrerenderOption {
    PRERENDER_OPTION_AUTO,
    PRERENDER_OPTION_DISABLED,
    PRERENDER_OPTION_ENABLED,
    PRERENDER_OPTION_PREFETCH_ONLY,
  };

  PrerenderOption prerender_option = PRERENDER_OPTION_AUTO;
  if (command_line.HasSwitch(switches::kPrerenderMode)) {
    const std::string switch_value =
        command_line.GetSwitchValueASCII(switches::kPrerenderMode);

    if (switch_value == switches::kPrerenderModeSwitchValueAuto) {
      prerender_option = PRERENDER_OPTION_AUTO;
    } else if (switch_value == switches::kPrerenderModeSwitchValueDisabled) {
      prerender_option = PRERENDER_OPTION_DISABLED;
    } else if (switch_value.empty() ||
               switch_value == switches::kPrerenderModeSwitchValueEnabled) {
      // The empty string means the option was provided with no value, and that
      // means enable.
      prerender_option = PRERENDER_OPTION_ENABLED;
    } else if (switch_value ==
               switches::kPrerenderModeSwitchValuePrefetchOnly) {
      prerender_option = PRERENDER_OPTION_PREFETCH_ONLY;
    } else {
      prerender_option = PRERENDER_OPTION_DISABLED;
      LOG(ERROR) << "Invalid --prerender option received on command line: "
                 << switch_value;
      LOG(ERROR) << "Disabling prerendering!";
    }
  }

  switch (prerender_option) {
    case PRERENDER_OPTION_AUTO:
      SetupPrefetchFieldTrial();
      SetupPrerenderFieldTrial();
      break;
    case PRERENDER_OPTION_DISABLED:
      PrerenderManager::SetIsPrefetchEnabled(false);
      PrerenderManager::SetMode(PrerenderManager::PRERENDER_MODE_DISABLED);
      break;
    case PRERENDER_OPTION_ENABLED:
      PrerenderManager::SetIsPrefetchEnabled(true);
      PrerenderManager::SetMode(PrerenderManager::PRERENDER_MODE_ENABLED);
      break;
    case PRERENDER_OPTION_PREFETCH_ONLY:
      PrerenderManager::SetIsPrefetchEnabled(true);
      PrerenderManager::SetMode(PrerenderManager::PRERENDER_MODE_DISABLED);
      break;
    default:
      NOTREACHED();
  }

  UMA_HISTOGRAM_ENUMERATION("Prerender.Sessions",
                            PrerenderManager::GetMode(),
                            PrerenderManager::PRERENDER_MODE_MAX);

  ConfigureOmniboxPrerender();
}

void ConfigureOmniboxPrerender() {
  // Field trial to see if we're enabled.
  const base::FieldTrial::Probability kDivisor = 100;

  base::FieldTrial::Probability kDisabledProbability = 10;
  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  if (channel == chrome::VersionInfo::CHANNEL_STABLE ||
      channel == chrome::VersionInfo::CHANNEL_BETA) {
    kDisabledProbability = 1;
  }
  scoped_refptr<base::FieldTrial> omnibox_prerender_trial(
      base::FieldTrialList::FactoryGetFieldTrial(
          kOmniboxTrialName, kDivisor, "OmniboxPrerenderEnabled",
          2012, 12, 30, &g_omnibox_trial_default_group_number));
  omnibox_prerender_trial->AppendGroup("OmniboxPrerenderDisabled",
                                       kDisabledProbability);

  // Field trial to set weighting of hits.
  const base::FieldTrial::Probability kFourProbability = 33;
  const base::FieldTrial::Probability kEightProbability = 33;

  scoped_refptr<base::FieldTrial> weighting_trial(
      base::FieldTrialList::FactoryGetFieldTrial(
          "OmniboxPrerenderHitWeightingTrial", kDivisor,
          "OmniboxPrerenderWeight1.0", 2012, 8, 30, NULL));
  const int kOmniboxWeightFourGroup =
      weighting_trial->AppendGroup("OmniboxPrerenderWeight4.0",
                                   kFourProbability);
  const int kOmniboxWeightEightGroup =
      weighting_trial->AppendGroup("OmniboxPrerenderWeight8.0",
                                   kEightProbability);
  const int group = weighting_trial->group();
  if (group == kOmniboxWeightFourGroup)
    predictors::AutocompleteActionPredictor::set_hit_weight(4.0);
  else if (group == kOmniboxWeightEightGroup)
    predictors::AutocompleteActionPredictor::set_hit_weight(8.0);
}

bool IsOmniboxEnabled(Profile* profile) {
  if (!profile || profile->IsOffTheRecord())
    return false;

  if (!PrerenderManager::IsPrerenderingPossible())
    return false;

  // Override any field trial groups if the user has set a command line flag.
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kPrerenderFromOmnibox)) {
    const std::string switch_value =
        CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kPrerenderFromOmnibox);

    if (switch_value == switches::kPrerenderFromOmniboxSwitchValueEnabled)
      return true;

    if (switch_value == switches::kPrerenderFromOmniboxSwitchValueDisabled)
      return false;

    DCHECK(switch_value == switches::kPrerenderFromOmniboxSwitchValueAuto);
  }

  const int group = base::FieldTrialList::FindValue(kOmniboxTrialName);
  return group == base::FieldTrial::kNotFinalized ||
         group == g_omnibox_trial_default_group_number;
}

}  // namespace prerender
