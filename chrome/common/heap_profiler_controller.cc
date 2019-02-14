// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/heap_profiler_controller.h"

#include <cmath>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/metrics_hashes.h"
#include "base/profiler/stack_sampling_profiler.h"
#include "base/rand_util.h"
#include "base/sampling_heap_profiler/module_cache.h"
#include "base/sampling_heap_profiler/sampling_heap_profiler.h"
#include "base/strings/string_number_conversions.h"
#include "base/task/post_task.h"
#include "components/metrics/call_stack_profile_builder.h"
#include "content/public/common/content_switches.h"

namespace {

constexpr char kMetadataSizeField[] = "HeapProfiler.AllocationInBytes";

constexpr base::Feature kSamplingHeapProfilerFeature{
    "SamplingHeapProfiler", base::FEATURE_DISABLED_BY_DEFAULT};

constexpr char kSamplingHeapProfilerFeatureSamplingRateKB[] =
    "sampling-rate-kb";

constexpr size_t kDefaultSamplingRateKB = 128;
constexpr base::TimeDelta kHeapCollectionInterval =
    base::TimeDelta::FromHours(24);

base::TimeDelta RandomInterval(base::TimeDelta mean) {
  // Time intervals between profile collections form a Poisson stream with
  // given mean interval.
  return -std::log(base::RandDouble()) * mean;
}

class SampleMetadataRecorder : public metrics::MetadataRecorder {
 public:
  SampleMetadataRecorder()
      : field_hash_(base::HashMetricName(kMetadataSizeField)) {}

  void SetCurrentSampleSize(size_t size) { current_sample_size_ = size; }

  std::pair<uint64_t, int64_t> GetHashAndValue() const override {
    return std::make_pair(field_hash_, current_sample_size_);
  }

 private:
  const uint64_t field_hash_;
  size_t current_sample_size_ = 0;
};

}  // namespace

HeapProfilerController::HeapProfilerController() = default;
HeapProfilerController::~HeapProfilerController() = default;

void HeapProfilerController::StartIfEnabled() {
  DCHECK(!started_);
  size_t sampling_rate_kb = 0;
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  if (command_line->HasSwitch(switches::kSamplingHeapProfiler)) {
    unsigned value;
    bool parsed = base::StringToUint(
        command_line->GetSwitchValueASCII(switches::kSamplingHeapProfiler),
        &value);
    sampling_rate_kb = parsed ? value : kDefaultSamplingRateKB;
  }

  bool on_trial = base::FeatureList::IsEnabled(kSamplingHeapProfilerFeature);
  if (on_trial && !sampling_rate_kb) {
    sampling_rate_kb = std::max(
        base::GetFieldTrialParamByFeatureAsInt(
            kSamplingHeapProfilerFeature,
            kSamplingHeapProfilerFeatureSamplingRateKB, kDefaultSamplingRateKB),
        0);
  }

  if (!sampling_rate_kb)
    return;

  started_ = true;
  auto* profiler = base::SamplingHeapProfiler::Get();
  profiler->SetSamplingInterval(sampling_rate_kb * 1024);
  profiler->Start();

  ScheduleNextSnapshot();
}

void HeapProfilerController::ScheduleNextSnapshot() {
  if (!task_runner_) {
    task_runner_ =
        base::CreateTaskRunnerWithTraits({base::TaskPriority::BEST_EFFORT});
  }
  task_runner_->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&HeapProfilerController::TakeSnapshot,
                     weak_factory_.GetWeakPtr()),
      RandomInterval(kHeapCollectionInterval));
}

void HeapProfilerController::TakeSnapshot() {
  RetrieveAndSendSnapshot();
  ScheduleNextSnapshot();
}

void HeapProfilerController::RetrieveAndSendSnapshot() {
  std::vector<base::SamplingHeapProfiler::Sample> samples =
      base::SamplingHeapProfiler::Get()->GetSamples(0);
  if (samples.empty())
    return;

  base::ModuleCache module_cache;
  SampleMetadataRecorder metadata_recorder;
  metrics::CallStackProfileParams params(
      metrics::CallStackProfileParams::BROWSER_PROCESS,
      metrics::CallStackProfileParams::UNKNOWN_THREAD,
      metrics::CallStackProfileParams::PERIODIC_HEAP_COLLECTION);
  metrics::CallStackProfileBuilder profile_builder(params, nullptr,
                                                   &metadata_recorder);

  for (const base::SamplingHeapProfiler::Sample& sample : samples) {
    std::vector<base::StackSamplingProfiler::Frame> frames;
    frames.reserve(sample.stack.size());
    for (const void* frame : sample.stack) {
      uintptr_t address = reinterpret_cast<uintptr_t>(frame);
      const base::ModuleCache::Module& module =
          module_cache.GetModuleForAddress(address);
      frames.emplace_back(address, module);
    }
    metadata_recorder.SetCurrentSampleSize(sample.total);
    profile_builder.RecordMetadata();
    profile_builder.OnSampleCompleted(std::move(frames));
  }

  profile_builder.OnProfileCompleted(base::TimeDelta(), base::TimeDelta());
}
