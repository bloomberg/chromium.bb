// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_BROWSER_FIELD_TRIALS_H_
#define CHROME_BROWSER_CHROME_BROWSER_FIELD_TRIALS_H_

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/gtest_prod_util.h"
#include "base/time.h"

class ChromeBrowserFieldTrials {
 public:
  explicit ChromeBrowserFieldTrials(const CommandLine& command_line);
  ~ChromeBrowserFieldTrials();

  // Called by the browser main sequence to set up Field Trials for this client.
  // |install_time| is the time this browser was installed (or the last time
  // prefs was reset). |install_time| is used by trials that are only created
  // for new installs of the browser.
  void SetupFieldTrials(const base::Time& install_time);

 private:
  FRIEND_TEST_ALL_PREFIXES(BrowserMainTest,
                           WarmConnectionFieldTrial_WarmestSocket);
  FRIEND_TEST_ALL_PREFIXES(BrowserMainTest, WarmConnectionFieldTrial_Random);
  FRIEND_TEST_ALL_PREFIXES(BrowserMainTest, WarmConnectionFieldTrial_Invalid);

  // Sets up common desktop-only field trials.
  // Add an invocation of your field trial init function to this method, or to
  // SetupFieldTrials if it is for all platforms.
  void SetupDesktopFieldTrials();

  // A/B test for spdy when --use-spdy not set.
  void SpdyFieldTrial();

  // A/B test for warmest socket vs. most recently used socket.
  void WarmConnectionFieldTrial();

  // Field trial to see what disabling DNS pre-resolution does to
  // latency of page loads.
  void PredictorFieldTrial();

  // A field trial to see what effects launching Chrome automatically on
  // computer startup has on retention and usage of Chrome.
  void AutoLaunchChromeFieldTrial();

  // A collection of one-time-randomized and session-randomized field trials
  // intended to test the uniformity and correctness of the field trial control,
  // bucketing and reporting systems.
  void SetupUniformityFieldTrials(const base::Time& install_date);

  // Disables the new tab field trial if not running in desktop mode.
  void DisableNewTabFieldTrialIfNecesssary();

  // Sets up the InfiniteCache field trial.
  void SetUpInfiniteCacheFieldTrial();

  // Sets up field trials for doing Cache Sensitivity Analysis.
  void SetUpCacheSensitivityAnalysisFieldTrial();

  // Disables the show profile switcher field trial if multi-profiles is not
  // enabled.
  void DisableShowProfileSwitcherTrialIfNecessary();

  // A field trial to determine the impact of using non-blocking reads for
  // TCP sockets on Windows instead of overlapped I/O.
  void WindowsOverlappedTCPReadsFieldTrial();

  // Instantiates dynamic trials by querying their state, to ensure they get
  // reported as used.
  void InstantiateDynamicTrials();

  const CommandLine& parsed_command_line_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserFieldTrials);
};

#endif  // CHROME_BROWSER_CHROME_BROWSER_FIELD_TRIALS_H_
