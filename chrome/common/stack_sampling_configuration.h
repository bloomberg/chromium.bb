// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_STACK_SAMPLING_CONFIGURATION_H_
#define CHROME_COMMON_STACK_SAMPLING_CONFIGURATION_H_

#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/profiler/stack_sampling_profiler.h"

namespace base {
class CommandLine;
}  // namespace base

// StackSamplingConfiguration chooses a configuration for the enable state of
// the stack sampling profiler across all processes. This configuration is
// determined once at browser process startup. Configurations for child
// processes are communicated via command line arguments.
class StackSamplingConfiguration {
 public:
  StackSamplingConfiguration();

  // Get the stack sampling params to use for this process.
  base::StackSamplingProfiler::SamplingParams
      GetSamplingParamsForCurrentProcess() const;

  // Returns true if the profiler should be started for the current process.
  //
  // This method must first be called from the main thread while the process is
  // single-threaded. It is thereafter safe to call it concurrently from other
  // threads.
  //
  // This method can be used via the following pattern:
  // if
  // (StackSamplingConfiguration::Get()->IsProfilerEnabledForCurrentProcess()) {
  //   // start the profiler.
  // }
  //
  // This method is currently called unsynchronized from two different threads
  // (UI thread and IO thread). This is okay due to the following reasons.
  //
  // 1) The profile configuration (which is constant) is guaranteed to have
  // already been created when this method is called from the IO thread, because
  // of the access for the UI thread sampling.
  //
  // 2) The only other data accessed in this method is the command line.
  bool IsProfilerEnabledForCurrentProcess() const;

  // Get the synthetic field trial configuration. Returns true if a synthetic
  // field trial should be registered. This should only be called from the
  // browser process. When run at startup, the profiler must use a synthetic
  // field trial since it runs before the metrics field trials are initialized.
  bool GetSyntheticFieldTrial(std::string* trial_name,
                              std::string* group_name) const;

  // Add a command line switch that instructs the child process to run the
  // profiler. This should only be called from the browser process.
  void AppendCommandLineSwitchForChildProcess(
      const std::string& process_type,
      base::CommandLine* command_line) const;

  // Returns the StackSamplingConfiguration for the process.
  static StackSamplingConfiguration* Get();

 private:
  // Configuration to use for this Chrome instance.
  enum ProfileConfiguration {
    // Chrome-wide configurations set in the browser process.
    PROFILE_DISABLED,
    PROFILE_CONTROL,
    PROFILE_ENABLED,

    // Configuration set in the child processes, which receive their enable
    // state on the command line from the browser process.
    PROFILE_FROM_COMMAND_LINE
  };

  // Configuration variations, along with weights to use when randomly choosing
  // one of a set of variations.
  struct Variation {
    ProfileConfiguration config;
    int weight;
  };

  // Randomly chooses a configuration from the weighted variations. Weights are
  // expected to sum to 100 as a sanity check.
  static ProfileConfiguration ChooseConfiguration(
      const std::vector<Variation>& variations);

  // Generates sampling profiler configurations for all processes.
  static ProfileConfiguration GenerateConfiguration();

  // In the browser process this represents the configuration to use across all
  // Chrome processes. In the child processes it is always
  // PROFILE_FROM_COMMAND_LINE.
  const ProfileConfiguration configuration_;

  DISALLOW_COPY_AND_ASSIGN(StackSamplingConfiguration);
};

#endif  // CHROME_COMMON_STACK_SAMPLING_CONFIGURATION_H_
