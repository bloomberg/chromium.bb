// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_STACK_SAMPLING_CONFIGURATION_H_
#define CHROME_BROWSER_STACK_SAMPLING_CONFIGURATION_H_

#include "base/profiler/stack_sampling_profiler.h"

// Chooses a configuration for the stack sampling profiler for browser process
// startup. This must live outside of ChromeBrowserMainParts so it can be
// friended by ChromeMetricsServiceAccessor.
class StackSamplingConfiguration {
 public:
  StackSamplingConfiguration();

  // Get the stack sampling params to use for this session.
  base::StackSamplingProfiler::SamplingParams GetSamplingParams() const;

  // Returns true if the profiler should be started at all.
  bool IsProfilerEnabled() const;

  // Register the chosen configuration as a synthetic field trial.
  void RegisterSyntheticFieldTrial() const;

 private:
  enum ProfileConfiguration {
    PROFILE_DISABLED,
    PROFILE_CONTROL,
    PROFILE_NO_SAMPLES,  // Run the profiler thread, but don't collect profiles.
    PROFILE_5HZ,
    PROFILE_10HZ,
    PROFILE_100HZ
  };

  static ProfileConfiguration GenerateConfiguration();

  const ProfileConfiguration configuration_;

  DISALLOW_COPY_AND_ASSIGN(StackSamplingConfiguration);
};

#endif  // CHROME_BROWSER_STACK_SAMPLING_CONFIGURATION_H_
