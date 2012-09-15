// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_BROWSER_FIELD_TRIALS_H_
#define CHROME_BROWSER_CHROME_BROWSER_FIELD_TRIALS_H_

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/gtest_prod_util.h"

class ChromeBrowserFieldTrials {
 public:
  explicit ChromeBrowserFieldTrials(const CommandLine& command_line);
  ~ChromeBrowserFieldTrials();

  // Add an invocation of your field trial init function to this method.
  void SetupFieldTrials(bool proxy_policy_is_set);

 private:
  FRIEND_TEST_ALL_PREFIXES(BrowserMainTest,
                           WarmConnectionFieldTrial_WarmestSocket);
  FRIEND_TEST_ALL_PREFIXES(BrowserMainTest, WarmConnectionFieldTrial_Random);
  FRIEND_TEST_ALL_PREFIXES(BrowserMainTest, WarmConnectionFieldTrial_Invalid);

  // A/B test for the maximum number of persistent connections per host.
  void ConnectionFieldTrial();

  // A/B test for determining a value for unused socket timeout.
  void SocketTimeoutFieldTrial();

  // A/B test for the maximum number of connections per proxy server.
  void ProxyConnectionsFieldTrial();

  // A/B test for spdy when --use-spdy not set.
  void SpdyFieldTrial();

  // A/B test for warmest socket vs. most recently used socket.
  void WarmConnectionFieldTrial();

  // A/B test for automatically establishing a backup TCP connection when a
  // specified timeout value is reached.
  void ConnectBackupJobsFieldTrial();

  // Field trial to see what disabling DNS pre-resolution does to
  // latency of page loads.
  void PredictorFieldTrial();

  // Field trial to see what effect installing defaults in the NTP apps pane
  // has on retention and general apps/webstore usage.
  void DefaultAppsFieldTrial();

  // A field trial to see what effects launching Chrome automatically on
  // computer startup has on retention and usage of Chrome.
  void AutoLaunchChromeFieldTrial();

  // A collection of one-time-randomized and session-randomized field trials
  // intended to test the uniformity and correctness of the field trial control,
  // bucketing and reporting systems.
  void SetupUniformityFieldTrials();

  // Disables the new tab field trial if not running in desktop mode.
  void DisableNewTabFieldTrialIfNecesssary();

  // Sets up the Safe Browsing interstitial redesign trial.
  void SetUpSafeBrowsingInterstitialFieldTrial();

  // Sets up the InfiniteCache field trial.
  void SetUpInfiniteCacheFieldTrial();

  const CommandLine& parsed_command_line_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserFieldTrials);
};

#endif  // CHROME_BROWSER_CHROME_BROWSER_FIELD_TRIALS_H_
