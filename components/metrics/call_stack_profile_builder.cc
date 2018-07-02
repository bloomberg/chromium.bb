// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/call_stack_profile_builder.h"

#include <utility>

#include "base/logging.h"

using StackSamplingProfiler = base::StackSamplingProfiler;

CallStackProfileBuilder::CallStackProfileBuilder(
    const StackSamplingProfiler::CompletedCallback& callback)
    : callback_(callback) {}

CallStackProfileBuilder::~CallStackProfileBuilder() = default;

void CallStackProfileBuilder::RecordAnnotations() {
  // The code inside this method must not do anything that could acquire a
  // mutex, including allocating memory (which includes LOG messages) because
  // that mutex could be held by a stopped thread, thus resulting in deadlock.
  sample_.process_milestones = StackSamplingProfiler::ProcessMilestone();
}

void CallStackProfileBuilder::OnSampleCompleted(
    std::vector<StackSamplingProfiler::InternalFrame> internal_frames) {
  DCHECK(sample_.frames.empty());

  // Dedup modules and convert InternalFrames to Frames.
  for (const auto& internal_frame : internal_frames) {
    const StackSamplingProfiler::InternalModule& module(
        internal_frame.internal_module);
    if (!module.is_valid) {
      sample_.frames.emplace_back(internal_frame.instruction_pointer,
                                  base::kUnknownModuleIndex);
      continue;
    }

    auto loc = module_index_.find(module.base_address);
    if (loc == module_index_.end()) {
      profile_.modules.emplace_back(module.base_address, module.id,
                                    module.filename);
      size_t index = profile_.modules.size() - 1;
      loc = module_index_.insert(std::make_pair(module.base_address, index))
                .first;
    }
    sample_.frames.emplace_back(internal_frame.instruction_pointer,
                                loc->second);
  }

  profile_.samples.push_back(std::move(sample_));
  sample_ = StackSamplingProfiler::Sample();
}

void CallStackProfileBuilder::OnProfileCompleted(
    base::TimeDelta profile_duration,
    base::TimeDelta sampling_period) {
  profile_.profile_duration = profile_duration;
  profile_.sampling_period = sampling_period;

  // Run the associated callback, passing the collected profile.
  callback_.Run(std::move(profile_));
}