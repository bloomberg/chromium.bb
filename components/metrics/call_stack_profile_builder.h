// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_CALL_STACK_PROFILE_BUILDER_H_
#define COMPONENTS_METRICS_CALL_STACK_PROFILE_BUILDER_H_

#include <map>
#include <vector>

#include "base/callback.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "base/sampling_heap_profiler/module_cache.h"
#include "base/time/time.h"
#include "components/metrics/call_stack_profile_params.h"
#include "components/metrics/child_call_stack_profile_collector.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

namespace metrics {

// An instance of the class is meant to be passed to base::StackSamplingProfiler
// to collect profiles. The profiles collected are uploaded via the metrics log.
//
// This uses the new StackSample encoding rather than the legacy Sample
// encoding.
class CallStackProfileBuilder
    : public base::StackSamplingProfiler::ProfileBuilder {
 public:
  // |completed_callback| is made when sampling a profile completes. Other
  // threads, including the UI thread, may block on callback completion so this
  // should run as quickly as possible.
  //
  // IMPORTANT NOTE: The callback is invoked on a thread the profiler
  // constructs, rather than on the thread used to construct the profiler, and
  // thus the callback must be callable on any thread.
  explicit CallStackProfileBuilder(
      const CallStackProfileParams& profile_params,
      base::OnceClosure completed_callback = base::OnceClosure());

  ~CallStackProfileBuilder() override;

  // base::StackSamplingProfiler::ProfileBuilder:
  void OnSampleCompleted(
      std::vector<base::StackSamplingProfiler::Frame> frames) override;
  void OnProfileCompleted(base::TimeDelta profile_duration,
                          base::TimeDelta sampling_period) override;

  // The function is used by sampling heap profiler. Its samples already come
  // with different counts.
  void OnSampleCompleted(std::vector<base::StackSamplingProfiler::Frame> frames,
                         size_t count);

  // Sets the callback to use for reporting browser process profiles. This
  // indirection is required to avoid a dependency on unnecessary metrics code
  // in child processes.
  static void SetBrowserProcessReceiverCallback(
      const base::RepeatingCallback<void(base::TimeTicks, SampledProfile)>&
          callback);

  // Sets the CallStackProfileCollector interface from |browser_interface|.
  // This function must be called within child processes.
  static void SetParentProfileCollectorForChildProcess(
      metrics::mojom::CallStackProfileCollectorPtr browser_interface);

 protected:
  // Test seam.
  virtual void PassProfilesToMetricsProvider(SampledProfile sampled_profile);

 private:
  // The functor for Stack comparison.
  struct StackComparer {
    bool operator()(const CallStackProfile::Stack* stack1,
                    const CallStackProfile::Stack* stack2) const;
  };

  // The SampledProfile protobuf message which contains the collected stack
  // samples.
  SampledProfile sampled_profile_;

  // The indexes of stacks, indexed by stack's address.
  std::map<const CallStackProfile::Stack*, int, StackComparer> stack_index_;

  // The indexes of modules, indexed by module's base_address.
  std::map<uintptr_t, size_t> module_index_;

  // The distinct modules in the current profile.
  std::vector<base::ModuleCache::Module> modules_;

  // Callback made when sampling a profile completes.
  base::OnceClosure completed_callback_;

  // The start time of a profile collection.
  const base::TimeTicks profile_start_time_;

  DISALLOW_COPY_AND_ASSIGN(CallStackProfileBuilder);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_CALL_STACK_PROFILE_BUILDER_H_
