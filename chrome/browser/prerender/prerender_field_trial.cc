// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_field_trial.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "chrome/browser/metrics/metrics_service.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"

namespace prerender {

int omnibox_original_group_id = 0;
int omnibox_conservative_group_id = 0;

// If the command line contains the --prerender-from-omnibox switch, enable
// prerendering from the Omnibox. If not, enter the user into a field trial.
void ConfigurePrerenderFromOmnibox();

void ConfigurePrefetchAndPrerender(const CommandLine& command_line) {
  enum PrerenderOption {
    PRERENDER_OPTION_AUTO,
    PRERENDER_OPTION_DISABLED,
    PRERENDER_OPTION_ENABLED,
    PRERENDER_OPTION_PREFETCH_ONLY,
  };

  PrerenderOption prerender_option = PRERENDER_OPTION_AUTO;
  if (command_line.HasSwitch(switches::kPrerender)) {
    const std::string switch_value =
        command_line.GetSwitchValueASCII(switches::kPrerender);

    if (switch_value == switches::kPrerenderSwitchValueAuto) {
      prerender_option = PRERENDER_OPTION_AUTO;
    } else if (switch_value == switches::kPrerenderSwitchValueDisabled) {
      prerender_option = PRERENDER_OPTION_DISABLED;
    } else if (switch_value.empty() ||
               switch_value == switches::kPrerenderSwitchValueEnabled) {
      // The empty string means the option was provided with no value, and that
      // means enable.
      prerender_option = PRERENDER_OPTION_ENABLED;
    } else if (switch_value == switches::kPrerenderSwitchValuePrefetchOnly) {
      prerender_option = PRERENDER_OPTION_PREFETCH_ONLY;
    } else {
      prerender_option = PRERENDER_OPTION_DISABLED;
      LOG(ERROR) << "Invalid --prerender option received on command line: "
                 << switch_value;
      LOG(ERROR) << "Disabling prerendering!";
    }
  }

  switch (prerender_option) {
    case PRERENDER_OPTION_AUTO: {
      const base::FieldTrial::Probability kPrerenderDivisor = 1000;
      const base::FieldTrial::Probability kPrerenderExp1Probability = 495;
      const base::FieldTrial::Probability kPrerenderControl1Probability = 5;
      const base::FieldTrial::Probability kPrerenderExp2Probability = 495;
      const base::FieldTrial::Probability kPrerenderControl2Probability = 5;
      COMPILE_ASSERT(
          kPrerenderExp1Probability + kPrerenderControl1Probability +
          kPrerenderExp2Probability + kPrerenderControl2Probability ==
          kPrerenderDivisor,
          PrerenderFieldTrial_numerator_matches_denominator);
      scoped_refptr<base::FieldTrial> trial(
          new base::FieldTrial("Prefetch", kPrerenderDivisor,
                               "ContentPrefetchPrerender1", 2012, 6, 30));

      const int kPrerenderExperiment1Group = trial->kDefaultGroupNumber;
      const int kPrerenderControl1Group =
          trial->AppendGroup("ContentPrefetchPrerenderControl1",
                             kPrerenderControl1Probability);
      const int kPrerenderExperiment2Group =
          trial->AppendGroup("ContentPrefetchPrerender2",
                             kPrerenderExp2Probability);
      const int kPrerenderControl2Group =
          trial->AppendGroup("ContentPrefetchPrerenderControl2",
                             kPrerenderControl2Probability);
      const int trial_group = trial->group();
      if (trial_group == kPrerenderExperiment1Group ||
                 trial_group == kPrerenderExperiment2Group) {
        ResourceDispatcherHost::set_is_prefetch_enabled(false);
        PrerenderManager::SetMode(
            PrerenderManager::PRERENDER_MODE_EXPERIMENT_PRERENDER_GROUP);
      } else if (trial_group == kPrerenderControl1Group ||
                 trial_group == kPrerenderControl2Group) {
        ResourceDispatcherHost::set_is_prefetch_enabled(false);
        PrerenderManager::SetMode(
            PrerenderManager::PRERENDER_MODE_EXPERIMENT_CONTROL_GROUP);
      } else {
        NOTREACHED();
      }
      break;
    }
    case PRERENDER_OPTION_DISABLED:
      ResourceDispatcherHost::set_is_prefetch_enabled(false);
      PrerenderManager::SetMode(PrerenderManager::PRERENDER_MODE_DISABLED);
      break;
    case PRERENDER_OPTION_ENABLED:
      ResourceDispatcherHost::set_is_prefetch_enabled(false);
      PrerenderManager::SetMode(PrerenderManager::PRERENDER_MODE_ENABLED);
      break;
    case PRERENDER_OPTION_PREFETCH_ONLY:
      ResourceDispatcherHost::set_is_prefetch_enabled(true);
      PrerenderManager::SetMode(PrerenderManager::PRERENDER_MODE_DISABLED);
      break;
    default:
      NOTREACHED();
  }

  UMA_HISTOGRAM_ENUMERATION("Prerender.Sessions",
                            PrerenderManager::GetMode(),
                            PrerenderManager::PRERENDER_MODE_MAX);

  ConfigurePrerenderFromOmnibox();
}

void ConfigurePrerenderFromOmnibox() {
  // Field trial to see if we're enabled.
  const base::FieldTrial::Probability kDivisor = 100;
  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kPrerenderFromOmnibox)) {
    const base::FieldTrial::Probability kEnabledProbability = 40;
    scoped_refptr<base::FieldTrial> trial(
        new base::FieldTrial("PrerenderFromOmnibox", kDivisor,
                             "OmniboxPrerenderDisabled", 2012, 8, 30));
    trial->AppendGroup("OmniboxPrerenderEnabled", kEnabledProbability);
  }

  // Field trial to see which heuristic to use.
  const base::FieldTrial::Probability kConservativeProbability = 50;
  scoped_refptr<base::FieldTrial> trial(
      new base::FieldTrial("PrerenderFromOmniboxHeuristic", kDivisor,
                           "OriginalAlgorithm", 2012, 8, 30));
  omnibox_original_group_id = base::FieldTrial::kDefaultGroupNumber;
  omnibox_conservative_group_id =
      trial->AppendGroup("ConservativeAlgorithm", kConservativeProbability);
}

bool IsOmniboxEnabled(Profile* profile) {
  if (!profile || profile->IsOffTheRecord())
    return false;

  if (!PrerenderManager::IsPrerenderingPossible())
    return false;

  if (!CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kPrerenderFromOmnibox))
    return true;

  if (!MetricsServiceHelper::IsMetricsReportingEnabled())
    return false;

  const int group = base::FieldTrialList::FindValue("PrerenderFromOmnibox");
  return group != base::FieldTrial::kNotFinalized &&
         group != base::FieldTrial::kDefaultGroupNumber;
}

OmniboxHeuristic GetOmniboxHeuristicToUse() {
  const int group =
      base::FieldTrialList::FindValue("PrerenderFromOmniboxHeuristic");
  if (group == omnibox_original_group_id)
    return OMNIBOX_HEURISTIC_ORIGINAL;
  if (group == omnibox_conservative_group_id)
    return OMNIBOX_HEURISTIC_CONSERVATIVE;

  // If we don't have a group just return the original heuristic.
  return OMNIBOX_HEURISTIC_ORIGINAL;
}

}  // namespace prerender
