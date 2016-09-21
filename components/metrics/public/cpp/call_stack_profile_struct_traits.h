// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Defines StructTraits specializations for translating between mojo types and
// base::StackSamplingProfiler types, with data validity checks.

#ifndef COMPONENTS_METRICS_CALL_STACK_PROFILE_STRUCT_TRAITS_H_
#define COMPONENTS_METRICS_CALL_STACK_PROFILE_STRUCT_TRAITS_H_

#include <vector>

#include "base/files/file_path.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "components/metrics/public/interfaces/call_stack_profile_collector.mojom.h"

namespace mojo {

template <>
struct StructTraits<metrics::mojom::CallStackModuleDataView,
                    base::StackSamplingProfiler::Module> {
  static uint64_t base_address(
      const base::StackSamplingProfiler::Module& module) {
    return module.base_address;
  }
  static const std::string& id(
      const base::StackSamplingProfiler::Module& module) {
    return module.id;
  }
  static const base::FilePath& filename(
      const base::StackSamplingProfiler::Module& module) {
    return module.filename;
  }

  static bool Read(metrics::mojom::CallStackModuleDataView data,
                   base::StackSamplingProfiler::Module* out) {
    // Linux has the longest build id at 40 bytes.
    static const size_t kMaxIDSize = 40;

    std::string id;
    base::FilePath filename;
    if (!data.ReadId(&id) || id.size() > kMaxIDSize ||
        !data.ReadFilename(&filename))
      return false;

    *out =
        base::StackSamplingProfiler::Module(data.base_address(), id, filename);
    return true;
  }
};

template <>
struct StructTraits<metrics::mojom::CallStackFrameDataView,
                    base::StackSamplingProfiler::Frame> {
  static uint64_t instruction_pointer(
      const base::StackSamplingProfiler::Frame& frame) {
    return frame.instruction_pointer;
  }
  static uint64_t module_index(
      const base::StackSamplingProfiler::Frame& frame) {
    return frame.module_index ==
        base::StackSamplingProfiler::Frame::kUnknownModuleIndex ?
        static_cast<uint64_t>(-1) :
        frame.module_index;
  }

  static bool Read(metrics::mojom::CallStackFrameDataView data,
                   base::StackSamplingProfiler::Frame* out) {
    size_t module_index = data.module_index() == static_cast<uint64_t>(-1) ?
        base::StackSamplingProfiler::Frame::kUnknownModuleIndex :
        data.module_index();

    // We can't know whether the module_index field is valid at this point since
    // we don't have access to the number of modules here. This will be checked
    // in CallStackProfile's Read function below.
    *out = base::StackSamplingProfiler::Frame(data.instruction_pointer(),
                                              module_index);
    return true;
  }
};

template <>
struct StructTraits<metrics::mojom::CallStackProfileDataView,
                    base::StackSamplingProfiler::CallStackProfile> {
  static const std::vector<base::StackSamplingProfiler::Module>& modules(
      const base::StackSamplingProfiler::CallStackProfile& profile) {
    return profile.modules;
  }
  static const std::vector<base::StackSamplingProfiler::Sample>& samples(
      const base::StackSamplingProfiler::CallStackProfile& profile) {
    return profile.samples;
  }
  static const base::TimeDelta profile_duration(
      const base::StackSamplingProfiler::CallStackProfile& profile) {
    return profile.profile_duration;
  }
  static const base::TimeDelta sampling_period(
      const base::StackSamplingProfiler::CallStackProfile& profile) {
    return profile.sampling_period;
  }

  static bool ValidateSamples(
      std::vector<base::StackSamplingProfiler::Sample> samples,
      size_t module_count) {
    for (const base::StackSamplingProfiler::Sample& sample : samples) {
      for (const base::StackSamplingProfiler::Frame& frame : sample) {
        if (frame.module_index >= module_count &&
            frame.module_index !=
                base::StackSamplingProfiler::Frame::kUnknownModuleIndex)
          return false;
      }
    }
    return true;
  }

  static bool Read(metrics::mojom::CallStackProfileDataView data,
                   base::StackSamplingProfiler::CallStackProfile* out) {
    std::vector<base::StackSamplingProfiler::Module> modules;
    std::vector<base::StackSamplingProfiler::Sample> samples;
    base::TimeDelta profile_duration, sampling_period;
    if (!data.ReadModules(&modules) || !data.ReadSamples(&samples) ||
        !data.ReadProfileDuration(&profile_duration) ||
        !data.ReadSamplingPeriod(&sampling_period) ||
        !ValidateSamples(samples, modules.size()))
      return false;

    *out = base::StackSamplingProfiler::CallStackProfile();
    out->modules = std::move(modules);
    out->samples = std::move(samples);
    out->profile_duration = profile_duration;
    out->sampling_period = sampling_period;
    return true;
  }
};

template <>
struct EnumTraits<metrics::mojom::SampleOrderingSpec,
                  metrics::CallStackProfileParams::SampleOrderingSpec> {

  static metrics::mojom::SampleOrderingSpec ToMojom(
      metrics::CallStackProfileParams::SampleOrderingSpec spec) {
    switch (spec) {
      case metrics::CallStackProfileParams::SampleOrderingSpec::MAY_SHUFFLE:
        return metrics::mojom::SampleOrderingSpec::MAY_SHUFFLE;
      case metrics::CallStackProfileParams::SampleOrderingSpec::PRESERVE_ORDER:
        return metrics::mojom::SampleOrderingSpec::PRESERVE_ORDER;
    }
    NOTREACHED();
    return metrics::mojom::SampleOrderingSpec::MAY_SHUFFLE;
  }

  static bool FromMojom(
      metrics::mojom::SampleOrderingSpec spec,
      metrics::CallStackProfileParams::SampleOrderingSpec* out) {
    switch (spec) {
      case metrics::mojom::SampleOrderingSpec::MAY_SHUFFLE:
        *out = metrics::CallStackProfileParams::SampleOrderingSpec::MAY_SHUFFLE;
        return true;
      case metrics::mojom::SampleOrderingSpec::PRESERVE_ORDER:
        *out =
            metrics::CallStackProfileParams::SampleOrderingSpec::PRESERVE_ORDER;
        return true;
    }
    return false;
  }
};

template <>
struct EnumTraits<metrics::mojom::Trigger,
                  metrics::CallStackProfileParams::Trigger> {
  static metrics::mojom::Trigger ToMojom(
      metrics::CallStackProfileParams::Trigger trigger) {
    switch (trigger) {
      case metrics::CallStackProfileParams::Trigger::UNKNOWN:
        return metrics::mojom::Trigger::UNKNOWN;
      case metrics::CallStackProfileParams::Trigger::PROCESS_STARTUP:
        return metrics::mojom::Trigger::PROCESS_STARTUP;
      case metrics::CallStackProfileParams::Trigger::JANKY_TASK:
        return metrics::mojom::Trigger::JANKY_TASK;
      case metrics::CallStackProfileParams::Trigger::THREAD_HUNG:
        return metrics::mojom::Trigger::THREAD_HUNG;
    }
    NOTREACHED();
    return metrics::mojom::Trigger::UNKNOWN;
  }

  static bool FromMojom(metrics::mojom::Trigger trigger,
                        metrics::CallStackProfileParams::Trigger* out) {
    switch (trigger) {
      case metrics::mojom::Trigger::UNKNOWN:
        *out = metrics::CallStackProfileParams::Trigger::UNKNOWN;
        return true;
      case metrics::mojom::Trigger::PROCESS_STARTUP:
        *out = metrics::CallStackProfileParams::Trigger::PROCESS_STARTUP;
        return true;
      case metrics::mojom::Trigger::JANKY_TASK:
        *out = metrics::CallStackProfileParams::Trigger::JANKY_TASK;
        return true;
      case metrics::mojom::Trigger::THREAD_HUNG:
        *out = metrics::CallStackProfileParams::Trigger::THREAD_HUNG;
        return true;
    }
    return false;
  }
};

template <>
struct StructTraits<metrics::mojom::CallStackProfileParamsDataView,
                    metrics::CallStackProfileParams> {
  static metrics::CallStackProfileParams::Trigger trigger(
      const metrics::CallStackProfileParams& params) {
    return params.trigger;
  }
  static metrics::CallStackProfileParams::SampleOrderingSpec ordering_spec(
      const metrics::CallStackProfileParams& params) {
    return params.ordering_spec;
  }

  static bool Read(metrics::mojom::CallStackProfileParamsDataView data,
                   metrics::CallStackProfileParams* out) {
    metrics::CallStackProfileParams::Trigger trigger;
    metrics::CallStackProfileParams::SampleOrderingSpec ordering_spec;
    if (!data.ReadTrigger(&trigger) || !data.ReadOrderingSpec(&ordering_spec))
      return false;
    *out = metrics::CallStackProfileParams(trigger, ordering_spec);
    return true;
  }
};

}  // mojo

#endif  // COMPONENTS_METRICS_CALL_STACK_PROFILE_STRUCT_TRAITS_H_
