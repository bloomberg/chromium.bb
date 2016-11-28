// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/prerender/prerender_field_trial.h"

#include <string>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/metrics/field_trial.h"
#include "chrome/browser/prerender/prerender_manager.h"
#include "chrome/common/chrome_switches.h"

namespace prerender {

void ConfigurePrerender(const base::CommandLine& command_line) {
  PrerenderManager::PrerenderManagerMode mode =
      PrerenderManager::PRERENDER_MODE_ENABLED;
  if (command_line.HasSwitch(switches::kPrerenderMode)) {
    const std::string switch_value =
        command_line.GetSwitchValueASCII(switches::kPrerenderMode);

    if (switch_value == switches::kPrerenderModeSwitchValueDisabled) {
      mode = PrerenderManager::PRERENDER_MODE_DISABLED;
    } else if (switch_value == switches::kPrerenderModeSwitchValuePrefetch) {
      mode = PrerenderManager::PRERENDER_MODE_NOSTATE_PREFETCH;
    } else if (switch_value.empty() ||
               switch_value == switches::kPrerenderModeSwitchValueEnabled) {
      // The empty string means the option was provided with no value, and that
      // means enable.
      mode = PrerenderManager::PRERENDER_MODE_ENABLED;
    } else if (switch_value == switches::kPrerenderModeSwitchValueSimpleLoad) {
      mode = PrerenderManager::PRERENDER_MODE_SIMPLE_LOAD_EXPERIMENT;
    } else {
      mode = PrerenderManager::PRERENDER_MODE_DISABLED;
      LOG(ERROR) << "Invalid --prerender option received on command line: "
                 << switch_value;
      LOG(ERROR) << "Disabling prerendering!";
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
