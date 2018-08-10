// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_CALL_STACK_PROFILE_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_CALL_STACK_PROFILE_METRICS_PROVIDER_H_

#include "base/feature_list.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "components/metrics/call_stack_profile_builder.h"
#include "components/metrics/metrics_provider.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

namespace metrics {

class ChromeUserMetricsExtension;

// Performs metrics logging for the stack sampling profiler.
class CallStackProfileMetricsProvider : public MetricsProvider {
 public:
  CallStackProfileMetricsProvider();
  ~CallStackProfileMetricsProvider() override;

  // Returns a callback for use with CallStackProfileBuilder that sets up
  // parameters for general browser process sampling. The callback should be
  // immediately passed to the CallStackProfileBuilder, and should not be
  // reused.
  static CallStackProfileBuilder::CompletedCallback
  GetProfilerCallbackForBrowserProcess();

  // Provides completed stack profile to the metrics provider. Intended for use
  // when receiving profiles over IPC. In-process StackSamplingProfiler users
  // should instead use a variant of GetProfilerCallback*(). |profile| is not
  // const& because it must be passed with std::move.
  static void ReceiveCompletedProfile(base::TimeTicks profile_start_time,
                                      SampledProfile profile);

  // MetricsProvider:
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
