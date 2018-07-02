// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_METRICS_CALL_STACK_PROFILE_BUILDER_H_
#define COMPONENTS_METRICS_CALL_STACK_PROFILE_BUILDER_H_

#include "base/profiler/stack_sampling_profiler.h"

#include <map>

// CallStackProfileBuilder builds a CallStackProfile from the collected sampling
// data.
//
// The results of the profile building -- a CallStackProfile, is passed to the
// completed callback. A CallStackProfile contains a set of Samples and
// Modules, and other sampling information. One Sample corresponds to a single
// recorded stack, and the Modules record those modules associated with the
// recorded stack frames.
class CallStackProfileBuilder
    : public base::StackSamplingProfiler::ProfileBuilder {
 public:
  CallStackProfileBuilder(
      const base::StackSamplingProfiler::CompletedCallback& callback);

  ~CallStackProfileBuilder() override;

  // base::StackSamplingProfiler::ProfileBuilder:
  void RecordAnnotations() override;
  void OnSampleCompleted(std::vector<base::StackSamplingProfiler::InternalFrame>
                             internal_frames) override;
  void OnProfileCompleted(base::TimeDelta profile_duration,
                          base::TimeDelta sampling_period) override;

 private:
  // The collected stack samples.
  base::StackSamplingProfiler::CallStackProfile profile_;

  // The current sample being recorded.
  base::StackSamplingProfiler::Sample sample_;

  // The indexes of internal modules, indexed by module's base_address.
  std::map<uintptr_t, size_t> module_index_;

  // Callback made when sampling a profile completes.
  const base::StackSamplingProfiler::CompletedCallback callback_;

  DISALLOW_COPY_AND_ASSIGN(CallStackProfileBuilder);
};

#endif  // COMPONENTS_METRICS_CALL_STACK_PROFILE_BUILDER_H_
