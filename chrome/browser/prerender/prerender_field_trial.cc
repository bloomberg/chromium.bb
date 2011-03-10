// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_field_trial.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "content/browser/renderer_host/resource_dispatcher_host.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/common/chrome_switches.h"


namespace prerender {

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
#if defined(OS_CHROMEOS)
      // Prerender is temporarily disabled on CrOS.
      // http://crosbug.com/12483
      prerender_option = PRERENDER_OPTION_DISABLED;
#else
      prerender_option = PRERENDER_OPTION_AUTO;
#endif
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
      const base::FieldTrial::Probability kPrefetchDivisor = 1000;
      const base::FieldTrial::Probability kYesPrefetchProbability = 0;
      const base::FieldTrial::Probability kPrerenderExperimentProbability = 50;
      const base::FieldTrial::Probability kPrerenderControlProbability = 50;

      scoped_refptr<base::FieldTrial> trial(
          new base::FieldTrial("Prefetch", kPrefetchDivisor,
                               "ContentPrefetchDisabled", 2011, 6, 30));

      const int kNoPrefetchGroup = trial->kDefaultGroupNumber;
      const int kYesPrefetchGroup =
          trial->AppendGroup("ContentPrefetchEnabled", kYesPrefetchProbability);
      const int kPrerenderExperimentGroup =
          trial->AppendGroup("ContentPrefetchPrerender",
                             kPrerenderExperimentProbability);
      const int kPrerenderControlGroup =
          trial->AppendGroup("ContentPrefetchPrerenderControl",
                             kPrerenderControlProbability);
      const int trial_group = trial->group();
      if (trial_group == kYesPrefetchGroup) {
        ResourceDispatcherHost::set_is_prefetch_enabled(true);
        PrerenderManager::SetMode(PrerenderManager::PRERENDER_MODE_DISABLED);
      } else if (trial_group == kNoPrefetchGroup) {
        ResourceDispatcherHost::set_is_prefetch_enabled(false);
        PrerenderManager::SetMode(
            PrerenderManager::PRERENDER_MODE_DISABLED);
      } else if (trial_group == kPrerenderExperimentGroup) {
        ResourceDispatcherHost::set_is_prefetch_enabled(true);
        PrerenderManager::SetMode(
            PrerenderManager::PRERENDER_MODE_EXPERIMENT_PRERENDER_GROUP);
      } else if (trial_group == kPrerenderControlGroup) {
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
      ResourceDispatcherHost::set_is_prefetch_enabled(true);
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
}

}  // namespace prerender
