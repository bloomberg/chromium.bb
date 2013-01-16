// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chrome_browser_field_trials.h"

#include <string>

#include "base/command_line.h"
#include "base/metrics/field_trial.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/sys_string_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/auto_launch_trial.h"
#include "chrome/browser/autocomplete/autocomplete_field_trial.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/gpu/chrome_gpu_util.h"
#include "chrome/browser/metrics/variations/variations_service.h"
#include "chrome/browser/prerender/prerender_field_trial.h"
#include "chrome/browser/safe_browsing/safe_browsing_blocking_page.h"
#include "chrome/browser/ui/sync/one_click_signin_helper.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/metrics/variations/variations_util.h"
#include "net/socket/client_socket_pool_base.h"
#include "net/spdy/spdy_session.h"
#include "ui/base/layout.h"

#if defined(OS_WIN)
#include "net/socket/tcp_client_socket_win.h"
#include "ui/base/win/dpi.h"  // For DisableNewTabFieldTrialIfNecesssary.
#endif  // defined(OS_WIN)

namespace {

// Set up a uniformity field trial. |one_time_randomized| indicates if the
// field trial is one-time randomized or session-randomized. |trial_name_string|
// must contain a "%d" since the percentage of the group will be inserted in
// the trial name. |num_trial_groups| must be a divisor of 100 (e.g. 5, 20)
void SetupSingleUniformityFieldTrial(
    bool one_time_randomized,
    const std::string& trial_name_string,
    const chrome_variations::VariationID trial_base_id,
    int num_trial_groups) {
  // Probability per group remains constant for all uniformity trials, what
  // changes is the probability divisor.
  static const base::FieldTrial::Probability kProbabilityPerGroup = 1;
  const std::string kDefaultGroupName = "default";
  const base::FieldTrial::Probability divisor = num_trial_groups;

  DCHECK_EQ(100 % num_trial_groups, 0);
  const int group_percent = 100 / num_trial_groups;
  const std::string trial_name = StringPrintf(trial_name_string.c_str(),
                                              group_percent);

  DVLOG(1) << "Trial name = " << trial_name;

  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial(
          trial_name, divisor, kDefaultGroupName, 2015, 1, 1, NULL));
  if (one_time_randomized)
    trial->UseOneTimeRandomization();
  chrome_variations::AssociateGoogleVariationID(
      chrome_variations::GOOGLE_WEB_PROPERTIES, trial_name, kDefaultGroupName,
      trial_base_id);
  // Loop starts with group 1 because the field trial automatically creates a
  // default group, which would be group 0.
  for (int group_number = 1; group_number < num_trial_groups; ++group_number) {
    const std::string group_name = StringPrintf("group_%02d", group_number);
    DVLOG(1) << "    Group name = " << group_name;
    trial->AppendGroup(group_name, kProbabilityPerGroup);
    chrome_variations::AssociateGoogleVariationID(
        chrome_variations::GOOGLE_WEB_PROPERTIES, trial_name, group_name,
        static_cast<chrome_variations::VariationID>(trial_base_id +
                                                    group_number));
  }

  // Now that all groups have been appended, call group() on the trial to
  // ensure that our trial is registered. This resolves an off-by-one issue
  // where the default group never gets chosen if we don't "use" the trial.
  const int chosen_group = trial->group();
  DVLOG(1) << "Chosen Group: " << chosen_group;
}

// Setup a 50% uniformity trial for new installs only. This is accomplished by
// disabling the trial on clients that were installed before a specified date.
void SetupNewInstallUniformityTrial(const base::Time& install_date) {
  const base::Time::Exploded kStartDate = {
    2012, 11, 0, 6,  // Nov 6, 2012
    0, 0, 0, 0       // 00:00:00.000
  };
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial(
          "UMA-New-Install-Uniformity-Trial", 100, "Disabled",
          2015, 1, 1, NULL));
  trial->UseOneTimeRandomization();
  trial->AppendGroup("Control", 50);
  trial->AppendGroup("Experiment", 50);
  const base::Time start_date = base::Time::FromLocalExploded(kStartDate);
  if (install_date < start_date)
    trial->Disable();
  else
    trial->group();
}

void SetSocketReusePolicy(int warmest_socket_trial_group,
                          const int socket_policy[],
                          int num_groups) {
  const int* result = std::find(socket_policy, socket_policy + num_groups,
                                warmest_socket_trial_group);
  DCHECK_NE(result, socket_policy + num_groups)
      << "Not a valid socket reuse policy group";
  net::SetSocketReusePolicy(result - socket_policy);
}

}  // namespace

ChromeBrowserFieldTrials::ChromeBrowserFieldTrials(
    const CommandLine& parsed_command_line) :
        parsed_command_line_(parsed_command_line) {
}

ChromeBrowserFieldTrials::~ChromeBrowserFieldTrials() {
}

void ChromeBrowserFieldTrials::SetupFieldTrials(
    const base::Time& install_time) {
  prerender::ConfigurePrefetchAndPrerender(parsed_command_line_);
  SpdyFieldTrial();
  WarmConnectionFieldTrial();
  AutoLaunchChromeFieldTrial();
  gpu_util::InitializeCompositingFieldTrial();
  SetupUniformityFieldTrials(install_time);
  AutocompleteFieldTrial::Activate();
  DisableNewTabFieldTrialIfNecesssary();
  SetUpInfiniteCacheFieldTrial();
  SetUpCacheSensitivityAnalysisFieldTrial();
  WindowsOverlappedTCPReadsFieldTrial();
#if defined(ENABLE_ONE_CLICK_SIGNIN)
  OneClickSigninHelper::InitializeFieldTrial();
#endif
  InstantiateDynamicTrials();
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

// If --socket-reuse-policy is not specified, run an A/B test for choosing the
// warmest socket.
void ChromeBrowserFieldTrials::WarmConnectionFieldTrial() {
  const CommandLine& command_line = parsed_command_line_;
  if (command_line.HasSwitch(switches::kSocketReusePolicy)) {
    std::string socket_reuse_policy_str = command_line.GetSwitchValueASCII(
        switches::kSocketReusePolicy);
    int policy = -1;
    base::StringToInt(socket_reuse_policy_str, &policy);

    const int policy_list[] = { 0, 1, 2 };
    VLOG(1) << "Setting socket_reuse_policy = " << policy;
    SetSocketReusePolicy(policy, policy_list, arraysize(policy_list));
    return;
  }

  const base::FieldTrial::Probability kWarmSocketDivisor = 100;
  const base::FieldTrial::Probability kWarmSocketProbability = 33;

  // Default value is USE_LAST_ACCESSED_SOCKET.
  int last_accessed_socket = -1;

  // After January 30, 2013 builds, it will always be in default group.
  scoped_refptr<base::FieldTrial> warmest_socket_trial(
      base::FieldTrialList::FactoryGetFieldTrial(
          "WarmSocketImpact", kWarmSocketDivisor, "last_accessed_socket",
          2013, 1, 30, &last_accessed_socket));

  const int warmest_socket = warmest_socket_trial->AppendGroup(
      "warmest_socket", kWarmSocketProbability);
  const int warm_socket = warmest_socket_trial->AppendGroup(
      "warm_socket", kWarmSocketProbability);

  const int warmest_socket_trial_group = warmest_socket_trial->group();

  const int policy_list[] = { warmest_socket, warm_socket,
                              last_accessed_socket };
  SetSocketReusePolicy(warmest_socket_trial_group, policy_list,
                       arraysize(policy_list));
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

void ChromeBrowserFieldTrials::SetupUniformityFieldTrials(
    const base::Time& install_date) {
  // One field trial will be created for each entry in this array. The i'th
  // field trial will have |trial_sizes[i]| groups in it, including the default
  // group. Each group will have a probability of 1/|trial_sizes[i]|.
  const int num_trial_groups[] = { 100, 20, 10, 5, 2 };

  // Declare our variation ID bases along side this array so we can loop over it
  // and assign the IDs appropriately. So for example, the 1 percent experiments
  // should have a size of 100 (100/100 = 1).
  const chrome_variations::VariationID trial_base_ids[] = {
      chrome_variations::UNIFORMITY_1_PERCENT_BASE,
      chrome_variations::UNIFORMITY_5_PERCENT_BASE,
      chrome_variations::UNIFORMITY_10_PERCENT_BASE,
      chrome_variations::UNIFORMITY_20_PERCENT_BASE,
      chrome_variations::UNIFORMITY_50_PERCENT_BASE
  };

  const std::string kOneTimeRandomizedTrialName =
      "UMA-Uniformity-Trial-%d-Percent";
  for (size_t i = 0; i < arraysize(num_trial_groups); ++i) {
    SetupSingleUniformityFieldTrial(true, kOneTimeRandomizedTrialName,
                                       trial_base_ids[i], num_trial_groups[i]);
  }

  // Setup a 5% session-randomized uniformity trial.
  const std::string kSessionRandomizedTrialName =
      "UMA-Session-Randomized-Uniformity-Trial-%d-Percent";
  SetupSingleUniformityFieldTrial(false, kSessionRandomizedTrialName,
      chrome_variations::UNIFORMITY_SESSION_RANDOMIZED_5_PERCENT_BASE, 20);

  SetupNewInstallUniformityTrial(install_date);
}

void ChromeBrowserFieldTrials::DisableNewTabFieldTrialIfNecesssary() {
  // The new tab button field trial will get created in variations_service.cc
  // through the variations server. However, since there are no HiDPI assets
  // for it, disable it for non-desktop layouts.
  base::FieldTrial* trial = base::FieldTrialList::Find("NewTabButton");
  if (trial) {
    bool using_hidpi_assets = false;
#if defined(ENABLE_HIDPI) && defined(OS_WIN)
    // Mirrors logic in resource_bundle_win.cc.
    using_hidpi_assets = ui::GetDPIScale() > 1.5;
#endif
    if (ui::GetDisplayLayout() != ui::LAYOUT_DESKTOP || using_hidpi_assets)
      trial->Disable();
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
}
