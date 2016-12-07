// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_field_trial.h"

#include <string>

#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/common/chrome_switches.h"
#include "components/variations/variations_associated_data.h"

namespace prerender {

// NoStatePrefetch feature modes, to control the PrerenderManager mode using
// the base::Feature API and field trials.
const char kNoStatePrefetchFeatureModeParameterName[] = "mode";
const char kNoStatePrefetchFeatureModeParameterPrefetch[] = "no_state_prefetch";
const char kNoStatePrefetchFeatureModeParameterPrerender[] = "prerender";
const char kNoStatePrefetchFeatureModeParameterSimpleLoad[] = "simple_load";

const base::Feature kNoStatePrefetchFeature{"NoStatePrefetch",
                                            base::FEATURE_ENABLED_BY_DEFAULT};

void ConfigurePrerender() {
  PrerenderManager::PrerenderManagerMode mode =
      PrerenderManager::PRERENDER_MODE_ENABLED;
  if (!base::FeatureList::IsEnabled(kNoStatePrefetchFeature)) {
    mode = PrerenderManager::PRERENDER_MODE_DISABLED;
  } else {
    std::string mode_value = variations::GetVariationParamValueByFeature(
        kNoStatePrefetchFeature, kNoStatePrefetchFeatureModeParameterName);
    if (mode_value == kNoStatePrefetchFeatureModeParameterPrefetch) {
      mode = PrerenderManager::PRERENDER_MODE_NOSTATE_PREFETCH;
    } else if (mode_value.empty() ||
               mode_value == kNoStatePrefetchFeatureModeParameterPrerender) {
      // The empty string means the option was provided with no value, and that
      // means enable.
      mode = PrerenderManager::PRERENDER_MODE_ENABLED;
    } else if (mode_value == kNoStatePrefetchFeatureModeParameterSimpleLoad) {
      mode = PrerenderManager::PRERENDER_MODE_SIMPLE_LOAD_EXPERIMENT;
    } else {
      LOG(ERROR) << "Invalid prerender mode: " << mode_value;
      LOG(ERROR) << "Disabling prerendering!";
      mode = PrerenderManager::PRERENDER_MODE_DISABLED;
    }
  }

  PrerenderManager::SetMode(mode);
}

bool IsOmniboxEnabled(Profile* profile) {
  if (!profile)
    return false;

  if (!PrerenderManager::IsPrerenderingPossible())
    return false;

  // Override any field trial groups if the user has set a command line flag.
  if (base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kPrerenderFromOmnibox)) {
    const std::string switch_value =
        base::CommandLine::ForCurrentProcess()->GetSwitchValueASCII(
            switches::kPrerenderFromOmnibox);

    if (switch_value == switches::kPrerenderFromOmniboxSwitchValueEnabled)
      return true;

    if (switch_value == switches::kPrerenderFromOmniboxSwitchValueDisabled)
      return false;

    DCHECK_EQ(switches::kPrerenderFromOmniboxSwitchValueAuto, switch_value);
  }

  return (base::FieldTrialList::FindFullName("PrerenderFromOmnibox") !=
          "OmniboxPrerenderDisabled");
}

}  // namespace prerender
