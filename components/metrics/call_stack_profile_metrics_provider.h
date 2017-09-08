// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_CALL_STACK_PROFILE_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_CALL_STACK_PROFILE_METRICS_PROVIDER_H_

#include <vector>

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "components/metrics/call_stack_profile_params.h"
#include "components/metrics/metrics_provider.h"

namespace metrics {

class ChromeUserMetricsExtension;

// Internal to expose functions for testing.
namespace internal {

// Returns the process uptime as a TimeDelta.
base::TimeDelta GetUptime();

// Get a callback for use with StackSamplingProfiler that provides completed
// profiles to this object. The callback should be immediately passed to the
// StackSamplingProfiler, and should not be reused between
// StackSamplingProfilers. This function may be called on any thread.
base::StackSamplingProfiler::CompletedCallback GetProfilerCallback(
    CallStackProfileParams* params);

}  // namespace internal

// Performs metrics logging for the stack sampling profiler.
class CallStackProfileMetricsProvider : public MetricsProvider {
 public:
  // These milestones of a process lifetime can be passed as process "mile-
  // stones" to StackSmaplingProfile::SetProcessMilestone(). Be sure to update
  // the translation constants at the top of the .cc file when this is changed.
  enum Milestones : int {
    MAIN_LOOP_START,
    MAIN_NAVIGATION_START,
    MAIN_NAVIGATION_FINISHED,
    FIRST_NONEMPTY_PAINT,

    SHUTDOWN_START,

    MILESTONES_MAX_VALUE
  };

  CallStackProfileMetricsProvider();
  ~CallStackProfileMetricsProvider() override;

  // Returns a callback for use with StackSamplingProfiler that sets up
  // parameters for browser process startup sampling. The callback should be
  // immediately passed to the StackSamplingProfiler, and should not be reused.
  static base::StackSamplingProfiler::CompletedCallback
  GetProfilerCallbackForBrowserProcessStartup();

  // Provides completed stack profiles to the metrics provider. Intended for use
  // when receiving profiles over IPC. In-process StackSamplingProfiler users
  // should instead use a variant of GetProfilerCallback*(). |profiles| is not
  // const& because it must be passed with std::move.
  static void ReceiveCompletedProfiles(
      CallStackProfileParams* params,
      base::StackSamplingProfiler::CallStackProfiles profiles);

  // Whether periodic sampling is enabled via a trial.
  static bool IsPeriodicSamplingEnabled();

  // MetricsProvider:
  void Init() override;
  void OnRecordingEnabled() override;
  void OnRecordingDisabled() override;
  void ProvideCurrentSessionData(
      ChromeUserMetricsExtension* uma_proto) override;

 protected:
  // base::Feature for reporting profiles. Provided here for test use.
  static const base::Feature kEnableReporting;

  // Reset the static state to the defaults after startup.
  static void ResetStaticStateForTesting();

 private:
  DISALLOW_COPY_AND_ASSIGN(CallStackProfileMetricsProvider);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_CALL_STACK_PROFILE_METRICS_PROVIDER_H_
