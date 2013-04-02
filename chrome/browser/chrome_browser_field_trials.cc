// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_field_trials.h"

#include <string>

#include "apps/field_trial_names.h"
#include "apps/pref_names.h"
#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/prefs/pref_service.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/auto_launch_trial.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/gpu/chrome_gpu_util.h"
#include "chrome/browser/metrics/variations/variations_service.h"
#include "chrome/browser/omnibox/omnibox_field_trial.h"
#include "chrome/browser/prerender/prerender_field_trial.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/safe_browsing/safe_browsing_blocking_page.h"
#include "chrome/browser/ui/sync/one_click_signin_helper.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/metrics/variations/uniformity_field_trials.h"
#include "chrome/common/metrics/variations/variations_util.h"
#include "chrome/common/pref_names.h"
#include "net/spdy/spdy_session.h"
#include "ui/base/layout.h"

#if defined(OS_WIN)
#include "net/socket/tcp_client_socket_win.h"
#endif  // defined(OS_WIN)

ChromeBrowserFieldTrials::ChromeBrowserFieldTrials(
    const CommandLine& parsed_command_line)
    : parsed_command_line_(parsed_command_line) {
}

ChromeBrowserFieldTrials::~ChromeBrowserFieldTrials() {
}

void ChromeBrowserFieldTrials::SetupFieldTrials(PrefService* local_state) {
  const base::Time install_time = base::Time::FromTimeT(
      local_state->GetInt64(prefs::kInstallDate));
  DCHECK(!install_time.is_null());
  chrome_variations::SetupUniformityFieldTrials(install_time);
  SetUpSimpleCacheFieldTrial();
#if !defined(OS_ANDROID) && !defined(OS_IOS)
  SetupDesktopFieldTrials(local_state);
#endif  // defined(OS_ANDROID)

#if defined(OS_ANDROID) || defined(OS_IOS)
  SetupMobileFieldTrials();
#endif  // defined(OS_ANDROID) || defined(OS_IOS)
}


#if defined(OS_ANDROID) || defined(OS_IOS)
void ChromeBrowserFieldTrials::SetupMobileFieldTrials() {
  DataCompressionProxyFieldTrial();
}

// Governs the rollout of the compression proxy for Chrome on mobile platforms.
// Always enabled in DEV and BETA versions.
// Stable percentage will be controlled from server.
void ChromeBrowserFieldTrials::DataCompressionProxyFieldTrial() {
  const char kDataCompressionProxyFieldTrialName[] =
      "DataCompressionProxyRollout";
  const base::FieldTrial::Probability kDataCompressionProxyDivisor = 1000;
  const base::FieldTrial::Probability kDataCompressionProxyStable = 0;
  const char kEnabled[] = "Enabled";
  const char kDisabled[] = "Disabled";

  // Find out if this is a stable channel.
  const bool kIsStableChannel =
      chrome::VersionInfo::GetChannel() == chrome::VersionInfo::CHANNEL_STABLE;

  // Experiment enabled until Jan 1, 2015. By default, disabled.
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial(
          kDataCompressionProxyFieldTrialName, kDataCompressionProxyDivisor,
          kDisabled, 2015, 1, 1, NULL));

  // Non-stable channels will run with probability 1.
  const int kEnabledGroup = trial->AppendGroup(
      kEnabled,
      kIsStableChannel ?
          kDataCompressionProxyStable : kDataCompressionProxyDivisor);

  const int v = trial->group();
  VLOG(1) << "DataCompression proxy enabled group id: " << kEnabledGroup
          << ". Selected group id: " << v;
}
#endif  // defined(OS_ANDROID) || defined(OS_IOS)

void ChromeBrowserFieldTrials::SetupDesktopFieldTrials(
    PrefService* local_state) {
  prerender::ConfigurePrefetchAndPrerender(parsed_command_line_);
  SpdyFieldTrial();
  AutoLaunchChromeFieldTrial();
  gpu_util::InitializeCompositingFieldTrial();
  OmniboxFieldTrial::ActivateStaticTrials();
  SetUpInfiniteCacheFieldTrial();
  SetUpCacheSensitivityAnalysisFieldTrial();
  DisableShowProfileSwitcherTrialIfNecessary();
  WindowsOverlappedTCPReadsFieldTrial();
#if defined(ENABLE_ONE_CLICK_SIGNIN)
  OneClickSigninHelper::InitializeFieldTrial();
#endif
  InstantiateDynamicTrials();
  SetupAppLauncherFieldTrial(local_state);
}

void ChromeBrowserFieldTrials::SetupAppLauncherFieldTrial(
    PrefService* local_state) {
  if (base::FieldTrialList::FindFullName(apps::kLauncherPromoTrialName) ==
      apps::kResetShowLauncherPromoPrefGroupName) {
    local_state->SetBoolean(apps::prefs::kShowAppLauncherPromo, true);
  }
}

// When --use-spdy not set, users will be in A/B test for spdy.
// group A (npn_with_spdy): this means npn and spdy are enabled. In case server
//                          supports spdy, browser will use spdy.
// group B (npn_with_http): this means npn is enabled but spdy won't be used.
//                          Http is still used for all requests.
//           default group: no npn or spdy is involved. The "old" non-spdy
//                          chrome behavior.
void ChromeBrowserFieldTrials::SpdyFieldTrial() {
  // Setup SPDY CWND Field trial.
  const base::FieldTrial::Probability kSpdyCwndDivisor = 100;
  const base::FieldTrial::Probability kSpdyCwnd16 = 20;     // fixed at 16
  const base::FieldTrial::Probability kSpdyCwnd10 = 20;     // fixed at 10
  const base::FieldTrial::Probability kSpdyCwndMin16 = 20;  // no less than 16
  const base::FieldTrial::Probability kSpdyCwndMin10 = 20;  // no less than 10

  // After June 30, 2013 builds, it will always be in default group
  // (cwndDynamic).
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial(
          "SpdyCwnd", kSpdyCwndDivisor, "cwndDynamic", 2013, 6, 30, NULL));

  trial->AppendGroup("cwnd10", kSpdyCwnd10);
  trial->AppendGroup("cwnd16", kSpdyCwnd16);
  trial->AppendGroup("cwndMin16", kSpdyCwndMin16);
  trial->AppendGroup("cwndMin10", kSpdyCwndMin10);
}

void ChromeBrowserFieldTrials::AutoLaunchChromeFieldTrial() {
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

void ChromeBrowserFieldTrials::SetUpInfiniteCacheFieldTrial() {
  const base::FieldTrial::Probability kDivisor = 100;

  base::FieldTrial::Probability infinite_cache_probability = 0;

  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial("InfiniteCache", kDivisor,
                                                 "No", 2013, 12, 31, NULL));
  trial->UseOneTimeRandomization();
  trial->AppendGroup("Yes", infinite_cache_probability);
  trial->AppendGroup("Control", infinite_cache_probability);
}

void ChromeBrowserFieldTrials::DisableShowProfileSwitcherTrialIfNecessary() {
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

// Sets up the experiment. The actual cache backend choice is made in the net/
// internals by looking at the experiment state.
void ChromeBrowserFieldTrials::SetUpSimpleCacheFieldTrial() {
  if (parsed_command_line_.HasSwitch(switches::kUseSimpleCacheBackend)) {
    const std::string opt_value = parsed_command_line_.GetSwitchValueASCII(
        switches::kUseSimpleCacheBackend);
    if (LowerCaseEqualsASCII(opt_value, "off")) {
      // This is the default.
      return;
    }
    const base::FieldTrial::Probability kDivisor = 100;
    scoped_refptr<base::FieldTrial> trial(
        base::FieldTrialList::FactoryGetFieldTrial("SimpleCacheTrial", kDivisor,
                                                   "No", 2013, 12, 31, NULL));
    trial->UseOneTimeRandomization();
    if (LowerCaseEqualsASCII(opt_value, "on")) {
      trial->AppendGroup("Yes", 100);
      return;
    }
#if defined(OS_ANDROID)
    if (LowerCaseEqualsASCII(opt_value, "experiment")) {
      // TODO(pasko): Make this the default on Android when the simple cache
      // adds a few more necessary features. Also adjust the probability.
      const base::FieldTrial::Probability kSimpleCacheProbability = 1;
      trial->AppendGroup("Yes", kSimpleCacheProbability);
      trial->AppendGroup("Control", kSimpleCacheProbability);
      trial->group();
    }
#endif
  }
}

void ChromeBrowserFieldTrials::SetUpCacheSensitivityAnalysisFieldTrial() {
  const base::FieldTrial::Probability kDivisor = 100;

  base::FieldTrial::Probability sensitivity_analysis_probability = 0;

  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial("CacheSensitivityAnalysis",
                                                 kDivisor, "No",
                                                 2012, 12, 31, NULL));
  trial->AppendGroup("ControlA", sensitivity_analysis_probability);
  trial->AppendGroup("ControlB", sensitivity_analysis_probability);
  trial->AppendGroup("100A", sensitivity_analysis_probability);
  trial->AppendGroup("100B", sensitivity_analysis_probability);
  trial->AppendGroup("200A", sensitivity_analysis_probability);
  trial->AppendGroup("200B", sensitivity_analysis_probability);
  trial->AppendGroup("400A", sensitivity_analysis_probability);
  trial->AppendGroup("400B", sensitivity_analysis_probability);
}

void ChromeBrowserFieldTrials::WindowsOverlappedTCPReadsFieldTrial() {
#if defined(OS_WIN)
  if (parsed_command_line_.HasSwitch(switches::kOverlappedRead)) {
    std::string option =
        parsed_command_line_.GetSwitchValueASCII(switches::kOverlappedRead);
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

void ChromeBrowserFieldTrials::InstantiateDynamicTrials() {
  // Call |FindValue()| on the trials below, which may come from the server, to
  // ensure they get marked as "used" for the purposes of data reporting.
  base::FieldTrialList::FindValue("UMA-Dynamic-Binary-Uniformity-Trial");
  base::FieldTrialList::FindValue("UMA-Dynamic-Uniformity-Trial");
  base::FieldTrialList::FindValue("InstantDummy");
  base::FieldTrialList::FindValue("InstantChannel");
  base::FieldTrialList::FindValue("Test0PercentDefault");
  // Activate the autocomplete dynamic field trials.
  OmniboxFieldTrial::ActivateDynamicTrials();
}
