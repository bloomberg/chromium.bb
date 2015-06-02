// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_CALL_STACK_PROFILE_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_CALL_STACK_PROFILE_METRICS_PROVIDER_H_

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "components/metrics/metrics_provider.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace metrics {
class ChromeUserMetricsExtension;

// Performs metrics logging for the stack sampling profiler.
class CallStackProfileMetricsProvider : public MetricsProvider {
 public:
  // The event that triggered the profile collection.
  enum Trigger {
    UNKNOWN,
    PROCESS_STARTUP,
    JANKY_TASK,
    THREAD_HUNG
  };

  CallStackProfileMetricsProvider();
  ~CallStackProfileMetricsProvider() override;

  // MetricsProvider:
  void OnRecordingEnabled() override;
  void OnRecordingDisabled() override;
  void ProvideGeneralMetrics(ChromeUserMetricsExtension* uma_proto) override;

  // Appends |profiles| for use by the next invocation of ProvideGeneralMetrics,
  // rather than sourcing them from the StackSamplingProfiler.
  void AppendSourceProfilesForTesting(
      const std::vector<base::StackSamplingProfiler::CallStackProfile>&
          profiles);

 protected:
  // Finch field trial and group for reporting profiles. Provided here for test
  // use.
  static const char kFieldTrialName[];
  static const char kReportProfilesGroupName[];

 private:
  // Returns true if reporting of profiles is enabled according to the
  // controlling Finch field trial.
  static bool IsSamplingProfilingReportingEnabled();

  // Invoked by the profiler on another thread.
  static void ReceiveCompletedProfiles(
      scoped_refptr<base::SingleThreadTaskRunner> task_runner,
      base::WeakPtr<CallStackProfileMetricsProvider> provider,
      const base::StackSamplingProfiler::CallStackProfiles& profiles);
  void AppendCompletedProfiles(
      const base::StackSamplingProfiler::CallStackProfiles& profiles);

  base::StackSamplingProfiler::CallStackProfiles pending_profiles_;

  base::WeakPtrFactory<CallStackProfileMetricsProvider> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(CallStackProfileMetricsProvider);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_CALL_STACK_PROFILE_METRICS_PROVIDER_H_
