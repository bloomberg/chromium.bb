// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_field_trials_desktop.h"

#include <string>

#include "apps/field_trial_names.h"
#include "apps/pref_names.h"
#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/prefs/pref_service.h"
#include "base/string_util.h"
#include "chrome/browser/auto_launch_trial.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/gpu/chrome_gpu_util.h"
#include "chrome/browser/omnibox/omnibox_field_trial.h"
#include "chrome/browser/prerender/prerender_field_trial.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_blocking_page.h"
#include "chrome/browser/ui/sync/one_click_signin_helper.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/metrics/variations/variations_util.h"
#include "net/spdy/spdy_session.h"
#include "ui/base/layout.h"

#if defined(OS_WIN)
#include "net/socket/tcp_client_socket_win.h"
#endif  // defined(OS_WIN)

namespace chrome {

namespace {

void SetupAppLauncherFieldTrial(PrefService* local_state) {
  if (base::FieldTrialList::FindFullName(apps::kLauncherPromoTrialName) ==
      apps::kResetShowLauncherPromoPrefGroupName) {
    local_state->SetBoolean(apps::prefs::kShowAppLauncherPromo, true);
  }
}

void AutoLaunchChromeFieldTrial() {
  std::string brand;
  google_util::GetBrand(&brand);

  // Create a 100% field trial based on the brand code.
  if (auto_launch_trial::IsInExperimentGroup(brand)) {
    base::FieldTrialList::CreateFieldTrial(kAutoLaunchTrialName,
                                           kAutoLaunchTrialAutoLaunchGroup);
  } else if (auto_launch_trial::IsInControlGroup(brand)) {
    base::FieldTrialList::CreateFieldTrial(kAutoLaunchTrialName,
                                           kAutoLaunchTrialControlGroup);
  }
}

void SetUpInfiniteCacheFieldTrial() {
  const base::FieldTrial::Probability kDivisor = 100;

  base::FieldTrial::Probability infinite_cache_probability = 0;

  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial("InfiniteCache", kDivisor,
                                                 "No", 2013, 12, 31, NULL));
  trial->UseOneTimeRandomization();
  trial->AppendGroup("Yes", infinite_cache_probability);
  trial->AppendGroup("Control", infinite_cache_probability);
}

void DisableShowProfileSwitcherTrialIfNecessary() {
  // This trial is created by the VariationsService, but it needs to be disabled
  // if multi-profiles isn't enabled or if browser frame avatar menu is
  // always hidden (Chrome OS).
  bool avatar_menu_always_hidden = false;
#if defined(OS_CHROMEOS)
  avatar_menu_always_hidden = true;
#endif
  base::FieldTrial* trial = base::FieldTrialList::Find("ShowProfileSwitcher");
  if (trial && (!ProfileManager::IsMultipleProfilesEnabled() ||
                avatar_menu_always_hidden)) {
    trial->Disable();
  }
}

void SetUpCacheSensitivityAnalysisFieldTrial() {
  const base::FieldTrial::Probability kDivisor = 100;

  base::FieldTrial::Probability sensitivity_analysis_probability = 0;

#if defined(OS_ANDROID)
  switch (chrome::VersionInfo::GetChannel()) {
    case chrome::VersionInfo::CHANNEL_DEV:
      sensitivity_analysis_probability = 10;
      break;
    case chrome::VersionInfo::CHANNEL_BETA:
      sensitivity_analysis_probability = 5;
      break;
    case chrome::VersionInfo::CHANNEL_STABLE:
      sensitivity_analysis_probability = 1;
      break;
    default:
      break;
  }
#endif

  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial("CacheSensitivityAnalysis",
                                                 kDivisor, "No",
                                                 2013, 06, 15, NULL));
  trial->AppendGroup("ControlA", sensitivity_analysis_probability);
  trial->AppendGroup("ControlB", sensitivity_analysis_probability);
  trial->AppendGroup("100A", sensitivity_analysis_probability);
  trial->AppendGroup("100B", sensitivity_analysis_probability);
  trial->AppendGroup("200A", sensitivity_analysis_probability);
  trial->AppendGroup("200B", sensitivity_analysis_probability);
  // TODO(gavinp,rvargas): Re-add 400 group to field trial if results justify.
}

void WindowsOverlappedTCPReadsFieldTrial(
    const CommandLine& parsed_command_line) {
#if defined(OS_WIN)
  if (parsed_command_line.HasSwitch(switches::kOverlappedRead)) {
    std::string option =
        parsed_command_line.GetSwitchValueASCII(switches::kOverlappedRead);
    if (LowerCaseEqualsASCII(option, "off"))
      net::TCPClientSocketWin::DisableOverlappedReads();
  } else {
    const base::FieldTrial::Probability kDivisor = 2;  // 1 in 2 chance
    const base::FieldTrial::Probability kOverlappedReadProbability = 1;
    scoped_refptr<base::FieldTrial> overlapped_reads_trial(
        base::FieldTrialList::FactoryGetFieldTrial("OverlappedReadImpact",
            kDivisor, "OverlappedReadEnabled", 2013, 6, 1, NULL));
    int overlapped_reads_disabled_group =
        overlapped_reads_trial->AppendGroup("OverlappedReadDisabled",
                                            kOverlappedReadProbability);
    int assigned_group = overlapped_reads_trial->group();
    if (assigned_group == overlapped_reads_disabled_group)
      net::TCPClientSocketWin::DisableOverlappedReads();
  }
#endif
}

}  // namespace

void SetupDesktopFieldTrials(const CommandLine& parsed_command_line,
                             const base::Time& install_time,
                             PrefService* local_state) {
  prerender::ConfigurePrefetchAndPrerender(parsed_command_line);
  AutoLaunchChromeFieldTrial();
  gpu_util::InitializeCompositingFieldTrial();
  OmniboxFieldTrial::ActivateStaticTrials();
  SetUpInfiniteCacheFieldTrial();
  SetUpCacheSensitivityAnalysisFieldTrial();
  DisableShowProfileSwitcherTrialIfNecessary();
  WindowsOverlappedTCPReadsFieldTrial(parsed_command_line);
  SetupAppLauncherFieldTrial(local_state);
}

}  // namespace chrome
