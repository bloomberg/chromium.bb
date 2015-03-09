// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_field_trials_desktop.h"

#include <string>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_util.h"
#include "chrome/browser/auto_launch_trial.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/prerender/prerender_field_trial.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/safe_browsing/safe_browsing_blocking_page.h"
#include "chrome/browser/ui/app_list/app_list_util.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/variations/variations_util.h"
#include "content/public/common/content_constants.h"
#include "net/spdy/spdy_session.h"
#include "ui/base/layout.h"

namespace chrome {

namespace {

void AutoLaunchChromeFieldTrial() {
  std::string brand;
  google_brand::GetBrand(&brand);

  // Create a 100% field trial based on the brand code.
  if (auto_launch_trial::IsInExperimentGroup(brand)) {
    base::FieldTrialList::CreateFieldTrial(kAutoLaunchTrialName,
                                           kAutoLaunchTrialAutoLaunchGroup);
  } else if (auto_launch_trial::IsInControlGroup(brand)) {
    base::FieldTrialList::CreateFieldTrial(kAutoLaunchTrialName,
                                           kAutoLaunchTrialControlGroup);
  }
}

void SetupInfiniteCacheFieldTrial() {
  const base::FieldTrial::Probability kDivisor = 100;

  base::FieldTrial::Probability infinite_cache_probability = 0;

  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial(
          "InfiniteCache", kDivisor, "No", 2013, 12, 31,
          base::FieldTrial::ONE_TIME_RANDOMIZED, NULL));
  trial->AppendGroup("Yes", infinite_cache_probability);
  trial->AppendGroup("Control", infinite_cache_probability);
}

void DisableShowProfileSwitcherTrialIfNecessary() {
  // This trial is created by the VariationsService, but it needs to be disabled
  // if multi-profiles isn't enabled.
  base::FieldTrial* trial = base::FieldTrialList::Find("ShowProfileSwitcher");
  if (trial && !profiles::IsMultipleProfilesEnabled())
    trial->Disable();
}

}  // namespace

void SetupDesktopFieldTrials(const base::CommandLine& parsed_command_line,
                             PrefService* local_state) {
  prerender::ConfigurePrerender(parsed_command_line);
  AutoLaunchChromeFieldTrial();
  SetupInfiniteCacheFieldTrial();
  DisableShowProfileSwitcherTrialIfNecessary();
  SetupShowAppLauncherPromoFieldTrial(local_state);
}

}  // namespace chrome
