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
#include "chrome/browser/chrome_gpu_util.h"
#include "chrome/browser/extensions/default_apps_trial.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/instant/instant_field_trials.h"
#include "chrome/browser/net/predictor.h"
#include "chrome/browser/prerender/prerender_field_trial.h"
#include "chrome/browser/safe_browsing/safe_browsing_blocking_page.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/metrics/variations/variations_util.h"
#include "net/http/http_stream_factory.h"
#include "net/socket/client_socket_pool_base.h"
#include "net/socket/client_socket_pool_manager.h"
#include "net/spdy/spdy_session.h"
#include "net/spdy/spdy_session_pool.h"
#include "ui/base/layout.h"

#if defined(OS_WIN)
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
  chrome_variations::AssociateGoogleVariationID(trial_name, kDefaultGroupName,
      trial_base_id);
  // Loop starts with group 1 because the field trial automatically creates a
  // default group, which would be group 0.
  for (int group_number = 1; group_number < num_trial_groups; ++group_number) {
    const std::string group_name = StringPrintf("group_%02d", group_number);
    DVLOG(1) << "    Group name = " << group_name;
    trial->AppendGroup(group_name, kProbabilityPerGroup);
    chrome_variations::AssociateGoogleVariationID(trial_name, group_name,
        static_cast<chrome_variations::VariationID>(trial_base_id +
                                                    group_number));
  }

  // Now that all groups have been appended, call group() on the trial to
  // ensure that our trial is registered. This resolves an off-by-one issue
  // where the default group never gets chosen if we don't "use" the trial.
  const int chosen_group = trial->group();
  DVLOG(1) << "Chosen Group: " << chosen_group;
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

void ChromeBrowserFieldTrials::SetupFieldTrials(bool proxy_policy_is_set) {
  // Note: make sure to call ConnectionFieldTrial() before
  // ProxyConnectionsFieldTrial().
  ConnectionFieldTrial();
  SocketTimeoutFieldTrial();
  // If a policy is defining the number of active connections this field test
  // shoud not be performed.
  if (!proxy_policy_is_set)
    ProxyConnectionsFieldTrial();
  prerender::ConfigurePrefetchAndPrerender(parsed_command_line_);
  instant::SetupInstantFieldTrials();
  SpdyFieldTrial();
  ConnectBackupJobsFieldTrial();
  WarmConnectionFieldTrial();
  PredictorFieldTrial();
  DefaultAppsFieldTrial();
  AutoLaunchChromeFieldTrial();
  gpu_util::InitializeCompositingFieldTrial();
  SetupUniformityFieldTrials();
  AutocompleteFieldTrial::Activate();
  DisableNewTabFieldTrialIfNecesssary();
  SetUpSafeBrowsingInterstitialFieldTrial();
  SetUpChannelIDFieldTrial();
  SetUpInfiniteCacheFieldTrial();
}

// This is an A/B test for the maximum number of persistent connections per
// host. Currently Chrome, Firefox, and IE8 have this value set at 6. Safari
// uses 4, and Fasterfox (a plugin for Firefox that supposedly configures it to
// run faster) uses 8. We would like to see how much of an effect this value has
// on browsing. Too large a value might cause us to run into SYN flood detection
// mechanisms.
void ChromeBrowserFieldTrials::ConnectionFieldTrial() {
  const base::FieldTrial::Probability kConnectDivisor = 100;
  const base::FieldTrial::Probability kConnectProbability = 1;  // 1% prob.

  // This (6) is the current default value. Having this group declared here
  // makes it straightforward to modify |kConnectProbability| such that the same
  // probability value will be assigned to all the other groups, while
  // preserving the remainder of the of probability space to the default value.
  int connect_6 = -1;

  // After June 30, 2011 builds, it will always be in default group.
  scoped_refptr<base::FieldTrial> connect_trial(
      base::FieldTrialList::FactoryGetFieldTrial(
          "ConnCountImpact", kConnectDivisor, "conn_count_6", 2011, 6, 30,
          &connect_6));

  const int connect_5 = connect_trial->AppendGroup("conn_count_5",
                                                   kConnectProbability);
  const int connect_7 = connect_trial->AppendGroup("conn_count_7",
                                                   kConnectProbability);
  const int connect_8 = connect_trial->AppendGroup("conn_count_8",
                                                   kConnectProbability);
  const int connect_9 = connect_trial->AppendGroup("conn_count_9",
                                                   kConnectProbability);

  const int connect_trial_group = connect_trial->group();

  int max_sockets = 0;
  if (connect_trial_group == connect_5) {
    max_sockets = 5;
  } else if (connect_trial_group == connect_6) {
    max_sockets = 6;
  } else if (connect_trial_group == connect_7) {
    max_sockets = 7;
  } else if (connect_trial_group == connect_8) {
    max_sockets = 8;
  } else if (connect_trial_group == connect_9) {
    max_sockets = 9;
  } else {
    NOTREACHED();
  }
  net::ClientSocketPoolManager::set_max_sockets_per_group(
      net::HttpNetworkSession::NORMAL_SOCKET_POOL, max_sockets);
}

// A/B test for determining a value for unused socket timeout. Currently the
// timeout defaults to 10 seconds. Having this value set too low won't allow us
// to take advantage of idle sockets. Setting it to too high could possibly
// result in more ERR_CONNECTION_RESETs, since some servers will kill a socket
// before we time it out. Since these are "unused" sockets, we won't retry the
// connection and instead show an error to the user. So we need to be
// conservative here. We've seen that some servers will close the socket after
// as short as 10 seconds. See http://crbug.com/84313 for more details.
void ChromeBrowserFieldTrials::SocketTimeoutFieldTrial() {
  const base::FieldTrial::Probability kIdleSocketTimeoutDivisor = 100;
  // 1% probability for all experimental settings.
  const base::FieldTrial::Probability kSocketTimeoutProbability = 1;

  // After June 30, 2011 builds, it will always be in default group.
  int socket_timeout_10 = -1;
  scoped_refptr<base::FieldTrial> socket_timeout_trial(
      base::FieldTrialList::FactoryGetFieldTrial(
          "IdleSktToImpact", kIdleSocketTimeoutDivisor, "idle_timeout_10",
          2011, 6, 30, &socket_timeout_10));

  const int socket_timeout_5 =
      socket_timeout_trial->AppendGroup("idle_timeout_5",
                                        kSocketTimeoutProbability);
  const int socket_timeout_20 =
      socket_timeout_trial->AppendGroup("idle_timeout_20",
                                        kSocketTimeoutProbability);

  const int idle_to_trial_group = socket_timeout_trial->group();

  if (idle_to_trial_group == socket_timeout_5) {
    net::ClientSocketPool::set_unused_idle_socket_timeout(
        base::TimeDelta::FromSeconds(5));
  } else if (idle_to_trial_group == socket_timeout_10) {
    net::ClientSocketPool::set_unused_idle_socket_timeout(
        base::TimeDelta::FromSeconds(10));
  } else if (idle_to_trial_group == socket_timeout_20) {
    net::ClientSocketPool::set_unused_idle_socket_timeout(
        base::TimeDelta::FromSeconds(20));
  } else {
    NOTREACHED();
  }
}

void ChromeBrowserFieldTrials::ProxyConnectionsFieldTrial() {
  const base::FieldTrial::Probability kProxyConnectionsDivisor = 100;
  // 25% probability
  const base::FieldTrial::Probability kProxyConnectionProbability = 1;

  // This (32 connections per proxy server) is the current default value.
  // Declaring it here allows us to easily re-assign the probability space while
  // maintaining that the default group always has the remainder of the "share",
  // which allows for cleaner and quicker changes down the line if needed.
  int proxy_connections_32 = -1;

  // After June 30, 2011 builds, it will always be in default group.
  scoped_refptr<base::FieldTrial> proxy_connection_trial(
      base::FieldTrialList::FactoryGetFieldTrial(
          "ProxyConnectionImpact", kProxyConnectionsDivisor,
          "proxy_connections_32", 2011, 6, 30, &proxy_connections_32));

  // The number of max sockets per group cannot be greater than the max number
  // of sockets per proxy server.  We tried using 8, and it can easily
  // lead to total browser stalls.
  const int proxy_connections_16 =
      proxy_connection_trial->AppendGroup("proxy_connections_16",
                                          kProxyConnectionProbability);
  const int proxy_connections_64 =
      proxy_connection_trial->AppendGroup("proxy_connections_64",
                                          kProxyConnectionProbability);

  const int proxy_connections_trial_group = proxy_connection_trial->group();

  int max_sockets = 0;
  if (proxy_connections_trial_group == proxy_connections_16) {
    max_sockets = 16;
  } else if (proxy_connections_trial_group == proxy_connections_32) {
    max_sockets = 32;
  } else if (proxy_connections_trial_group == proxy_connections_64) {
    max_sockets = 64;
  } else {
    NOTREACHED();
  }
  net::ClientSocketPoolManager::set_max_sockets_per_proxy_server(
      net::HttpNetworkSession::NORMAL_SOCKET_POOL, max_sockets);
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

  if (parsed_command_line_.HasSwitch(switches::kMaxSpdyConcurrentStreams)) {
    int value = 0;
    base::StringToInt(parsed_command_line_.GetSwitchValueASCII(
            switches::kMaxSpdyConcurrentStreams),
        &value);
    if (value > 0)
      net::SpdySession::set_max_concurrent_streams(value);
  }
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

// If neither --enable-connect-backup-jobs or --disable-connect-backup-jobs is
// specified, run an A/B test for automatically establishing backup TCP
// connections when a certain timeout value is exceeded.
void ChromeBrowserFieldTrials::ConnectBackupJobsFieldTrial() {
  if (parsed_command_line_.HasSwitch(switches::kEnableConnectBackupJobs)) {
    net::internal::ClientSocketPoolBaseHelper::set_connect_backup_jobs_enabled(
        true);
  } else if (parsed_command_line_.HasSwitch(
        switches::kDisableConnectBackupJobs)) {
    net::internal::ClientSocketPoolBaseHelper::set_connect_backup_jobs_enabled(
        false);
  } else {
    const base::FieldTrial::Probability kConnectBackupJobsDivisor = 100;
    // 1% probability.
    const base::FieldTrial::Probability kConnectBackupJobsProbability = 1;
    // After June 30, 2011 builds, it will always be in default group.
    int connect_backup_jobs_enabled = -1;
    scoped_refptr<base::FieldTrial> trial(
        base::FieldTrialList::FactoryGetFieldTrial("ConnnectBackupJobs",
            kConnectBackupJobsDivisor, "ConnectBackupJobsEnabled",
            2011, 6, 30, &connect_backup_jobs_enabled));
    trial->AppendGroup("ConnectBackupJobsDisabled",
                       kConnectBackupJobsProbability);
    const int trial_group = trial->group();
    net::internal::ClientSocketPoolBaseHelper::set_connect_backup_jobs_enabled(
        trial_group == connect_backup_jobs_enabled);
  }
}

void ChromeBrowserFieldTrials::PredictorFieldTrial() {
  const base::FieldTrial::Probability kDivisor = 1000;
  // For each option (i.e., non-default), we have a fixed probability.
  // 0.1% probability.
  const base::FieldTrial::Probability kProbabilityPerGroup = 1;

  // After June 30, 2011 builds, it will always be in default group
  // (default_enabled_prefetch).
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial(
          "DnsImpact", kDivisor, "default_enabled_prefetch", 2011, 10, 30,
          NULL));

  // First option is to disable prefetching completely.
  int disabled_prefetch = trial->AppendGroup("disabled_prefetch",
                                              kProbabilityPerGroup);

  // We're running two experiments at the same time.  The first set of trials
  // modulates the delay-time until we declare a congestion event (and purge
  // our queue).  The second modulates the number of concurrent resolutions
  // we do at any time.  Users are in exactly one trial (or the default) during
  // any one run, and hence only one experiment at a time.
  // Experiment 1:
  // Set congestion detection at 250, 500, or 750ms, rather than the 1 second
  // default.
  int max_250ms_prefetch = trial->AppendGroup("max_250ms_queue_prefetch",
                                              kProbabilityPerGroup);
  int max_500ms_prefetch = trial->AppendGroup("max_500ms_queue_prefetch",
                                              kProbabilityPerGroup);
  int max_750ms_prefetch = trial->AppendGroup("max_750ms_queue_prefetch",
                                              kProbabilityPerGroup);
  // Set congestion detection at 2 seconds instead of the 1 second default.
  int max_2s_prefetch = trial->AppendGroup("max_2s_queue_prefetch",
                                           kProbabilityPerGroup);
  // Experiment 2:
  // Set max simultaneous resoultions to 2, 4, or 6, and scale the congestion
  // limit proportionally (so we don't impact average probability of asserting
  // congesion very much).
  int max_2_concurrent_prefetch = trial->AppendGroup(
      "max_2 concurrent_prefetch", kProbabilityPerGroup);
  int max_4_concurrent_prefetch = trial->AppendGroup(
      "max_4 concurrent_prefetch", kProbabilityPerGroup);
  int max_6_concurrent_prefetch = trial->AppendGroup(
      "max_6 concurrent_prefetch", kProbabilityPerGroup);

  if (trial->group() != disabled_prefetch) {
    // Initialize the DNS prefetch system.
    size_t max_parallel_resolves =
        chrome_browser_net::Predictor::kMaxSpeculativeParallelResolves;
    int max_queueing_delay_ms =
        chrome_browser_net::Predictor::kMaxSpeculativeResolveQueueDelayMs;

    if (trial->group() == max_2_concurrent_prefetch)
      max_parallel_resolves = 2;
    else if (trial->group() == max_4_concurrent_prefetch)
      max_parallel_resolves = 4;
    else if (trial->group() == max_6_concurrent_prefetch)
      max_parallel_resolves = 6;
    chrome_browser_net::Predictor::set_max_parallel_resolves(
        max_parallel_resolves);

    if (trial->group() == max_250ms_prefetch) {
      max_queueing_delay_ms =
         (250 * chrome_browser_net::Predictor::kTypicalSpeculativeGroupSize) /
         max_parallel_resolves;
    } else if (trial->group() == max_500ms_prefetch) {
      max_queueing_delay_ms =
          (500 * chrome_browser_net::Predictor::kTypicalSpeculativeGroupSize) /
          max_parallel_resolves;
    } else if (trial->group() == max_750ms_prefetch) {
      max_queueing_delay_ms =
          (750 * chrome_browser_net::Predictor::kTypicalSpeculativeGroupSize) /
          max_parallel_resolves;
    } else if (trial->group() == max_2s_prefetch) {
      max_queueing_delay_ms =
          (2000 * chrome_browser_net::Predictor::kTypicalSpeculativeGroupSize) /
          max_parallel_resolves;
    }
    chrome_browser_net::Predictor::set_max_queueing_delay(
        max_queueing_delay_ms);
  }
}

void ChromeBrowserFieldTrials::DefaultAppsFieldTrial() {
  std::string brand;
  google_util::GetBrand(&brand);

  // Create a 100% field trial based on the brand code.
  if (LowerCaseEqualsASCII(brand, "ecdb")) {
    base::FieldTrialList::CreateFieldTrial(kDefaultAppsTrialName,
                                           kDefaultAppsTrialNoAppsGroup);
  } else if (LowerCaseEqualsASCII(brand, "ecda")) {
    base::FieldTrialList::CreateFieldTrial(kDefaultAppsTrialName,
                                           kDefaultAppsTrialWithAppsGroup);
  }
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

void ChromeBrowserFieldTrials::SetupUniformityFieldTrials() {
  // One field trial will be created for each entry in this array. The i'th
  // field trial will have |trial_sizes[i]| groups in it, including the default
  // group. Each group will have a probability of 1/|trial_sizes[i]|.
  const int num_trial_groups[] = { 100, 20, 10, 5, 2 };

  // Declare our variation ID bases along side this array so we can loop over it
  // and assign the IDs appropriately. So for example, the 1 percent experiments
  // should have a size of 100 (100/100 = 1).
  const chrome_variations::VariationID trial_base_ids[] = {
      chrome_variations::kUniformity1PercentBase,
      chrome_variations::kUniformity5PercentBase,
      chrome_variations::kUniformity10PercentBase,
      chrome_variations::kUniformity20PercentBase,
      chrome_variations::kUniformity50PercentBase
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
      chrome_variations::kUniformitySessionRandomized5PercentBase, 20);
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

void ChromeBrowserFieldTrials::SetUpSafeBrowsingInterstitialFieldTrial() {
  const base::FieldTrial::Probability kDivisor = 100;
  const base::FieldTrial::Probability kVersion2Probability = 50;  // 50% prob.
  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial("SBInterstitial", kDivisor,
                                                 "V1", 2012, 9, 19, NULL));
  trial->UseOneTimeRandomization();
  trial->AppendGroup("V2", kVersion2Probability);
}

void ChromeBrowserFieldTrials::SetUpChannelIDFieldTrial() {
  chrome::VersionInfo::Channel channel = chrome::VersionInfo::GetChannel();
  if (channel == chrome::VersionInfo::CHANNEL_CANARY) {
    net::SSLConfigService::EnableChannelIDTrial();
  } else if (channel == chrome::VersionInfo::CHANNEL_DEV &&
             base::FieldTrialList::IsOneTimeRandomizationEnabled()) {
    const base::FieldTrial::Probability kDivisor = 100;
    // 10% probability of being in the enabled group.
    const base::FieldTrial::Probability kEnableProbability = 10;
    scoped_refptr<base::FieldTrial> trial =
        base::FieldTrialList::FactoryGetFieldTrial(
            "ChannelID", kDivisor, "disable", 2012, 11, 5, NULL);
    trial->UseOneTimeRandomization();
    int enable_group = trial->AppendGroup("enable", kEnableProbability);
    if (trial->group() == enable_group)
      net::SSLConfigService::EnableChannelIDTrial();
  }
}

void ChromeBrowserFieldTrials::SetUpInfiniteCacheFieldTrial() {
  const base::FieldTrial::Probability kDivisor = 100;

#if (defined(OS_CHROMEOS) || defined(OS_ANDROID) || defined(OS_IOS))
  const base::FieldTrial::Probability kInfiniteCacheProbability = 0;
#else
  const base::FieldTrial::Probability kInfiniteCacheProbability = 1;
#endif

  scoped_refptr<base::FieldTrial> trial(
      base::FieldTrialList::FactoryGetFieldTrial("InfiniteCache", kDivisor,
                                                 "No", 2013, 12, 31, NULL));
  trial->UseOneTimeRandomization();
  trial->AppendGroup("Yes", kInfiniteCacheProbability);
  trial->AppendGroup("Control", kInfiniteCacheProbability);
}
