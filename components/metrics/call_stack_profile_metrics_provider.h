// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_CALL_STACK_PROFILE_METRICS_PROVIDER_H_
#define COMPONENTS_METRICS_CALL_STACK_PROFILE_METRICS_PROVIDER_H_

#include <vector>

#include "base/profiler/stack_sampling_profiler.h"
#include "components/metrics/metrics_provider.h"

namespace metrics {
class ChromeUserMetricsExtension;

// Performs metrics logging for the stack sampling profiler.
class CallStackProfileMetricsProvider : public MetricsProvider {
 public:
  CallStackProfileMetricsProvider();
  ~CallStackProfileMetricsProvider() override;

  // MetricsProvider:
  void ProvideGeneralMetrics(ChromeUserMetricsExtension* uma_proto) override;

  // Uses |profiles| as the source data for the next invocation of
  // ProvideGeneralMetrics, rather than sourcing them from the
  // StackSamplingProfiler.
  void SetSourceProfilesForTesting(
      const std::vector<base::StackSamplingProfiler::CallStackProfile>&
          profiles);

 private:
  std::vector<base::StackSamplingProfiler::CallStackProfile>
      source_profiles_for_test_;

  DISALLOW_COPY_AND_ASSIGN(CallStackProfileMetricsProvider);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_CALL_STACK_PROFILE_METRICS_PROVIDER_H_
