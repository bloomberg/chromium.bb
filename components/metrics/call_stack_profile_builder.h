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
#include "components/metrics/call_stack_profile_params.h"
#include "third_party/metrics_proto/sampled_profile.pb.h"

namespace metrics {

// CallStackProfileBuilder builds a SampledProfile in the protocol buffer
// message format from the collected sampling data. The message is then passed
// to the completed callback.

// A SampledProfile message (third_party/metrics_proto/sampled_profile.proto)
// contains a CallStackProfile message
// (third_party/metrics_proto/call_stack_profile.proto) and associated profile
// parameters (process/thread/trigger event). A CallStackProfile message
// contains a set of Sample messages and ModuleIdentifier messages, and other
// sampling information. One Sample corresponds to a single recorded stack, and
// the ModuleIdentifiers record those modules associated with the recorded stack
// frames.
class CallStackProfileBuilder
    : public base::StackSamplingProfiler::ProfileBuilder {
 public:
  // The callback type used to collect a SampledProfile protocol buffer message.
  // The passed SampledProfile is move-only. Other threads, including the UI
  // thread, may block on callback completion so this should run as quickly as
  // possible.
  //
  // IMPORTANT NOTE: The callback is invoked on a thread the profiler
  // constructs, rather than on the thread used to construct the profiler, and
  // thus the callback must be callable on any thread. For threads with message
  // loops that create CallStackProfileBuilders, posting a task to the message
  // loop with the moved (i.e. std::move) profile is the thread-safe callback
  // implementation.
  using CompletedCallback = base::RepeatingCallback<void(SampledProfile)>;

  // Frame represents an individual sampled stack frame with module information.
  struct Frame {
    Frame(uintptr_t instruction_pointer, size_t module_index);
    ~Frame();

    // Default constructor to satisfy IPC macros. Do not use explicitly.
    Frame();

    // The sampled instruction pointer within the function.
    uintptr_t instruction_pointer;

    // Index of the module in the associated vector of mofules. We don't
    // represent module state directly here to save space.
    size_t module_index;
  };

  // Sample represents a set of stack frames with some extra information.
  struct Sample {
    Sample();
    Sample(const Sample& sample);
    ~Sample();

    // These constructors are used only during testing.
    Sample(const Frame& frame);
    Sample(const std::vector<Frame>& frames);

    // The entire stack frame when the sample is taken.
    std::vector<Frame> frames;

    // A bit-field indicating which process milestones have passed. This can be
    // used to tell where in the process lifetime the samples are taken. Just
    // as a "lifetime" can only move forward, these bits mark the milestones of
    // the processes life as they occur. Bits can be set but never reset. The
    // actual definition of the individual bits is left to the user of this
    // module.
    uint32_t process_milestones = 0;
  };

  // These milestones of a process lifetime can be passed as process "mile-
  // stones" to CallStackProfileBuilder::SetProcessMilestone(). Be sure to
  // update the translation constants at the top of the .cc file when this is
  // changed.
  enum Milestones : int {
    MAIN_LOOP_START,
    MAIN_NAVIGATION_START,
    MAIN_NAVIGATION_FINISHED,
    FIRST_NONEMPTY_PAINT,

    SHUTDOWN_START,

    MILESTONES_MAX_VALUE
  };

  CallStackProfileBuilder(const CompletedCallback& callback,
                          const CallStackProfileParams& profile_params);

  ~CallStackProfileBuilder() override;

  // base::StackSamplingProfiler::ProfileBuilder:
  void RecordAnnotations() override;
  void OnSampleCompleted(
      std::vector<base::StackSamplingProfiler::Frame> frames) override;
  void OnProfileCompleted(base::TimeDelta profile_duration,
                          base::TimeDelta sampling_period) override;

  // Sets the current system state that is recorded with each captured stack
  // frame. This is thread-safe so can be called from anywhere. The parameter
  // value should be from an enumeration of the appropriate type with values
  // ranging from 0 to 31, inclusive. This sets bits within Sample field of
  // |process_milestones|. The actual meanings of these bits are defined
  // (globally) by the caller(s).
  static void SetProcessMilestone(int milestone);

 private:
  // The collected stack samples in proto buffer message format.
  CallStackProfile proto_profile_;

  // The current sample being recorded.
  Sample sample_;

  // The indexes of samples, indexed by the sample.
  std::map<Sample, int> sample_index_;

  // The indexes of modules, indexed by module's base_address.
  std::map<uintptr_t, size_t> module_index_;

  // The distinct modules in the current profile.
  std::vector<base::ModuleCache::Module> modules_;

  // The process milestones of a previous sample.
  uint32_t milestones_ = 0;

  // Callback made when sampling a profile completes.
  const CompletedCallback callback_;

  // The parameters associated with the sampled profile.
  const CallStackProfileParams profile_params_;

  DISALLOW_COPY_AND_ASSIGN(CallStackProfileBuilder);
};

}  // namespace metrics

#endif  // COMPONENTS_METRICS_CALL_STACK_PROFILE_BUILDER_H_
