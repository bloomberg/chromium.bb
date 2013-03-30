// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROME_BROWSER_FIELD_TRIALS_H_
#define CHROME_BROWSER_CHROME_BROWSER_FIELD_TRIALS_H_

#include "base/basictypes.h"
#include "base/command_line.h"
#include "base/time.h"

class PrefService;

class ChromeBrowserFieldTrials {
 public:
  explicit ChromeBrowserFieldTrials(const CommandLine& command_line);
  ~ChromeBrowserFieldTrials();

  // Called by the browser main sequence to set up Field Trials for this client.
  // |local_state| is used to extract properties like install time.
  void SetupFieldTrials(PrefService* local_state);

 private:
  // Sets up common desktop-only field trials.
  // Add an invocation of your field trial init function to this method, or to
  // SetupFieldTrials if it is for all platforms.
  // |local_state| is needed by some other methods called from within this one.
  void SetupDesktopFieldTrials(PrefService* local_state);

  // This is not quite a field trial initialization, but it's an initialization
  // that depends on a field trial, so why not? :-)
  // |local_state| is needed to reset a local pref based on the chosen group.
  void SetupAppLauncherFieldTrial(PrefService* local_state);

  // A/B test for spdy when --use-spdy not set.
  void SpdyFieldTrial();

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

  // A field trial to check the simple cache performance.
  void SetUpSimpleCacheFieldTrial();

  // Instantiates dynamic trials by querying their state, to ensure they get
  // reported as used.
  void InstantiateDynamicTrials();

  const CommandLine& parsed_command_line_;

  DISALLOW_COPY_AND_ASSIGN(ChromeBrowserFieldTrials);
};

#endif  // CHROME_BROWSER_CHROME_BROWSER_FIELD_TRIALS_H_
