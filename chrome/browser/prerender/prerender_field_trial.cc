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

using base::FieldTrial;
using base::FieldTrialList;

namespace prerender {

namespace {

const char kOmniboxTrialName[] = "PrerenderFromOmnibox";
int g_omnibox_trial_default_group_number = kint32min;

const char kSpeculativePrefetchingLearningTrialName[] =
    "SpeculativePrefetchingLearning";
int g_speculative_prefetching_learning_default_group_number = kint32min;

void SetupPrefetchFieldTrial() {
  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  if (channel == chrome::VersionInfo::CHANNEL_STABLE ||
      channel == chrome::VersionInfo::CHANNEL_BETA) {
    return;
  }

  const FieldTrial::Probability divisor = 1000;
  const FieldTrial::Probability prefetch_probability = 500;
  scoped_refptr<FieldTrial> trial(
      FieldTrialList::FactoryGetFieldTrial(
          "Prefetch", divisor, "ContentPrefetchPrefetchOff",
          2013, 6, 30, NULL));
  const int kPrefetchOnGroup = trial->AppendGroup("ContentPrefetchPrefetchOn",
                                                  prefetch_probability);
  PrerenderManager::SetIsPrefetchEnabled(trial->group() == kPrefetchOnGroup);
}

void SetupPrerenderFieldTrial() {
  const FieldTrial::Probability divisor = 1000;

  FieldTrial::Probability control_probability;
  FieldTrial::Probability experiment_multi_prerender_probability;
  FieldTrial::Probability experiment_5min_ttl_probability;
  FieldTrial::Probability experiment_no_use_probability;

  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  if (channel == chrome::VersionInfo::CHANNEL_STABLE ||
      channel == chrome::VersionInfo::CHANNEL_BETA) {
    // Use very conservatives and stable settings in beta and stable.
    const FieldTrial::Probability release_prerender_enabled_probability = 980;
    const FieldTrial::Probability release_control_probability = 10;
    const FieldTrial::Probability
        release_experiment_multi_prerender_probability = 0;
    const FieldTrial::Probability release_experiment_5min_ttl_probability = 10;
    const FieldTrial::Probability release_experiment_no_use_probability = 0;
    COMPILE_ASSERT(
        release_prerender_enabled_probability + release_control_probability +
        release_experiment_multi_prerender_probability +
        release_experiment_5min_ttl_probability +
        release_experiment_no_use_probability == divisor,
        release_experiment_probabilities_must_equal_divisor);

    control_probability = release_experiment_5min_ttl_probability;
    experiment_multi_prerender_probability =
        release_experiment_multi_prerender_probability;
    experiment_5min_ttl_probability = release_control_probability;
    experiment_no_use_probability = release_experiment_no_use_probability;
  } else {
    // In testing channels, use more experiments and a larger control group to
    // improve quality of data.
    const FieldTrial::Probability dev_prerender_enabled_probability = 250;
    const FieldTrial::Probability dev_control_probability = 250;
    const FieldTrial::Probability
        dev_experiment_multi_prerender_probability = 250;
    const FieldTrial::Probability dev_experiment_5min_ttl_probability = 125;
    const FieldTrial::Probability dev_experiment_no_use_probability = 125;
    COMPILE_ASSERT(dev_prerender_enabled_probability + dev_control_probability +
                   dev_experiment_multi_prerender_probability +
                   dev_experiment_5min_ttl_probability +
                   dev_experiment_no_use_probability == divisor,
                   dev_experiment_probabilities_must_equal_divisor);

    control_probability = dev_experiment_5min_ttl_probability;
    experiment_multi_prerender_probability =
        dev_experiment_multi_prerender_probability;
    experiment_5min_ttl_probability = dev_control_probability;
    experiment_no_use_probability = dev_experiment_no_use_probability;
  }

  int prerender_enabled_group = -1;
  scoped_refptr<FieldTrial> trial(
      FieldTrialList::FactoryGetFieldTrial(
          "Prerender", divisor, "PrerenderEnabled",
          2013, 6, 30, &prerender_enabled_group));
  const int control_group =
      trial->AppendGroup("PrerenderControl",
                         control_probability);
  const int experiment_multi_prerender_group =
      trial->AppendGroup("PrerenderMulti",
                         experiment_multi_prerender_probability);
  const int experiment_5_min_TTL_group =
      trial->AppendGroup("Prerender5minTTL",
                         experiment_5min_ttl_probability);
  const int experiment_no_use_group =
      trial->AppendGroup("PrerenderNoUse",
                         experiment_no_use_probability);

  const int trial_group = trial->group();
  if (trial_group == prerender_enabled_group) {
    PrerenderManager::SetMode(
        PrerenderManager::PRERENDER_MODE_EXPERIMENT_PRERENDER_GROUP);
  } else if (trial_group == control_group) {
    PrerenderManager::SetMode(
        PrerenderManager::PRERENDER_MODE_EXPERIMENT_CONTROL_GROUP);
  } else if (trial_group == experiment_multi_prerender_group) {
    PrerenderManager::SetMode(
        PrerenderManager::PRERENDER_MODE_EXPERIMENT_MULTI_PRERENDER_GROUP);
  } else if (trial_group == experiment_5_min_TTL_group) {
    PrerenderManager::SetMode(
        PrerenderManager::PRERENDER_MODE_EXPERIMENT_5MIN_TTL_GROUP);
  } else if (trial_group == experiment_no_use_group) {
    PrerenderManager::SetMode(
        PrerenderManager::PRERENDER_MODE_EXPERIMENT_NO_USE_GROUP);
  } else {
    NOTREACHED();
  }
}

}  // end namespace

void ConfigureOmniboxPrerender();
void ConfigureSpeculativePrefetching();

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
  ConfigureSpeculativePrefetching();
}

void ConfigureOmniboxPrerender() {
  // Field trial to see if we're enabled.
  const FieldTrial::Probability kDivisor = 100;

  FieldTrial::Probability kDisabledProbability = 10;
  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  if (channel == chrome::VersionInfo::CHANNEL_STABLE ||
      channel == chrome::VersionInfo::CHANNEL_BETA) {
    kDisabledProbability = 1;
  }
  scoped_refptr<FieldTrial> omnibox_prerender_trial(
      FieldTrialList::FactoryGetFieldTrial(
          kOmniboxTrialName, kDivisor, "OmniboxPrerenderEnabled",
          2012, 12, 30, &g_omnibox_trial_default_group_number));
  omnibox_prerender_trial->AppendGroup("OmniboxPrerenderDisabled",
                                       kDisabledProbability);
}

bool IsOmniboxEnabled(Profile* profile) {
  if (!profile)
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

  const int group = FieldTrialList::FindValue(kOmniboxTrialName);
  return group == FieldTrial::kNotFinalized ||
         group == g_omnibox_trial_default_group_number;
}

void ConfigureSpeculativePrefetching() {
  // Field trial to see if we're enabled.
  const FieldTrial::Probability kDivisor = 100;

  FieldTrial::Probability kDisabledProbability = 10;
  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  if (channel == chrome::VersionInfo::CHANNEL_STABLE ||
      channel == chrome::VersionInfo::CHANNEL_BETA) {
    kDisabledProbability = 100;
  }
  scoped_refptr<FieldTrial> speculative_prefetching_learning_trial(
      FieldTrialList::FactoryGetFieldTrial(
          kSpeculativePrefetchingLearningTrialName,
          kDivisor,
          "SpeculativePrefetchingLearningEnabled",
          2012, 12, 30,
          &g_speculative_prefetching_learning_default_group_number));
  speculative_prefetching_learning_trial->AppendGroup(
      "SpeculativePrefetchingDisabled",
      kDisabledProbability);
}

bool IsSpeculativeResourcePrefetchingLearningEnabled(Profile* profile) {
  if (!profile || profile->IsOffTheRecord())
    return false;

  // Override any field trial groups if the user has set a command line flag.
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kSpeculativeResourcePrefetching)) {
    const std::string switch_value =
        CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kSpeculativeResourcePrefetching);

    return switch_value == switches::kSpeculativeResourcePrefetchingLearning;
  }

  const int group = FieldTrialList::FindValue(
      kSpeculativePrefetchingLearningTrialName);
  return group == g_speculative_prefetching_learning_default_group_number;
}

}  // namespace prerender
