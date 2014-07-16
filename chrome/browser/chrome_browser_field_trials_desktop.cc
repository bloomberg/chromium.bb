// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_field_trials_desktop.h"

#include <string>

#include "base/command_line.h"
#include "base/environment.h"
#include "base/metrics/field_trial.h"
#include "base/prefs/pref_service.h"
#include "base/strings/string_util.h"
#include "chrome/browser/auto_launch_trial.h"
#include "chrome/browser/google/google_brand.h"
#include "chrome/browser/omnibox/omnibox_field_trial.h"
#include "chrome/browser/prerender/prerender_field_trial.h"
#include "chrome/browser/profiles/profiles_state.h"
#include "chrome/browser/safe_browsing/safe_browsing_blocking_page.h"
#include "chrome/browser/ui/app_list/app_list_util.h"
#include "chrome/browser/ui/sync/one_click_signin_helper.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/pref_names.h"
#include "chrome/common/variations/variations_util.h"
#include "content/public/common/content_constants.h"
#include "net/spdy/spdy_session.h"
#include "ui/base/layout.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/user_manager.h"
#endif

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

void SetupPreReadFieldTrial() {
  // The chrome executable will have set (or not) an environment variable with
  // the group name into which this client belongs.
  std::string group;
  scoped_ptr<base::Environment> env(base::Environment::Create());
  if (!env->GetVar(chrome::kPreReadEnvironmentVariable, &group) ||
      group.empty()) {
    return;
  }

  // Initialize the field trial. We declare all of the groups here (so that
  // the dashboard creation tools can find them) but force the probability
  // of being assigned to the group already chosen by the executable, if any,
  // to 100%.
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial(
          "BrowserPreReadExperiment", 100, "100-pct-default",
          2014, 7, 1, base::FieldTrial::SESSION_RANDOMIZED, NULL));
  trial->AppendGroup("100-pct-control", group == "100-pct-control" ? 100 : 0);
  trial->AppendGroup("95-pct", group == "95-pct" ? 100 : 0);
  trial->AppendGroup("90-pct", group == "90-pct" ? 100 : 0);
  trial->AppendGroup("85-pct", group == "85-pct" ? 100 : 0);
  trial->AppendGroup("80-pct", group == "80-pct" ? 100 : 0);
  trial->AppendGroup("75-pct", group == "75-pct" ? 100 : 0);
  trial->AppendGroup("70-pct", group == "70-pct" ? 100 : 0);
  trial->AppendGroup("65-pct", group == "65-pct" ? 100 : 0);
  trial->AppendGroup("60-pct", group == "60-pct" ? 100 : 0);
  trial->AppendGroup("55-pct", group == "55-pct" ? 100 : 0);
  trial->AppendGroup("50-pct", group == "50-pct" ? 100 : 0);
  trial->AppendGroup("45-pct", group == "45-pct" ? 100 : 0);
  trial->AppendGroup("40-pct", group == "40-pct" ? 100 : 0);
  trial->AppendGroup("35-pct", group == "35-pct" ? 100 : 0);
  trial->AppendGroup("30-pct", group == "30-pct" ? 100 : 0);
  trial->AppendGroup("25-pct", group == "25-pct" ? 100 : 0);
  trial->AppendGroup("20-pct", group == "20-pct" ? 100 : 0);
  trial->AppendGroup("15-pct", group == "15-pct" ? 100 : 0);
  trial->AppendGroup("10-pct", group == "10-pct" ? 100 : 0);
  trial->AppendGroup("5-pct", group == "5-pct" ? 100 : 0);
  trial->AppendGroup("0-pct", group == "0-pct" ? 100 : 0);

  // We have to call group in order to mark the experiment as active.
  trial->group();
}

}  // namespace

void SetupDesktopFieldTrials(const CommandLine& parsed_command_line,
                             PrefService* local_state) {
  prerender::ConfigurePrerender(parsed_command_line);
  AutoLaunchChromeFieldTrial();
  OmniboxFieldTrial::ActivateStaticTrials();
  SetupInfiniteCacheFieldTrial();
  DisableShowProfileSwitcherTrialIfNecessary();
  SetupShowAppLauncherPromoFieldTrial(local_state);
  SetupPreReadFieldTrial();
}

}  // namespace chrome
