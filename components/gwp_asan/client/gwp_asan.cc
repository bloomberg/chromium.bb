// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/client/gwp_asan.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "base/allocator/buildflags.h"
#include "base/debug/crash_logging.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/field_trial_params.h"
#include "base/numerics/safe_math.h"
#include "base/optional.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "build/build_config.h"
#include "components/gwp_asan/client/guarded_page_allocator.h"

#if BUILDFLAG(USE_ALLOCATOR_SHIM)
#include "components/gwp_asan/client/sampling_malloc_shims.h"
#endif  // BUILDFLAG(USE_ALLOCATOR_SHIM)

namespace gwp_asan {

namespace internal {
namespace {

constexpr int kDefaultMaxAllocations = 35;
constexpr int kDefaultMaxMetadata = 150;

#if defined(ARCH_CPU_64_BITS)
constexpr int kDefaultTotalPages = 2048;
#else
// Use much less virtual memory on 32-bit builds (where OOMing due to lack of
// address space is a concern.)
constexpr int kDefaultTotalPages = kDefaultMaxMetadata * 2;
#endif

// The allocation sampling frequency is calculated using the formula:
// multiplier * range**rand
// where rand is a random real number in the range [0,1).
constexpr int kDefaultAllocationSamplingMultiplier = 1000;
constexpr int kDefaultAllocationSamplingRange = 64;

constexpr double kDefaultProcessSamplingProbability = 0.2;
// The multiplier to increase the ProcessSamplingProbability in scenarios where
// we want to perform additional testing (e.g. on canary/dev builds or in the
// browser process.) The multiplier increase is cumulative when multiple
// conditions apply.
constexpr int kDefaultProcessSamplingBoost = 4;

const base::Feature kGwpAsan{"GwpAsanMalloc", base::FEATURE_ENABLED_BY_DEFAULT};

// Returns whether this process should be sampled to enable GWP-ASan.
bool SampleProcess(const base::Feature& feature,
                   bool is_canary_dev,
                   bool is_browser_process) {
  double process_sampling_probability =
      GetFieldTrialParamByFeatureAsDouble(feature, "ProcessSamplingProbability",
                                          kDefaultProcessSamplingProbability);
  if (process_sampling_probability < 0.0 ||
      process_sampling_probability > 1.0) {
    DLOG(ERROR) << "GWP-ASan ProcessSamplingProbability is out-of-range: "
                << process_sampling_probability;
    return false;
  }

  int process_sampling_boost = GetFieldTrialParamByFeatureAsInt(
      feature, "ProcessSamplingBoost", kDefaultProcessSamplingBoost);
  base::CheckedNumeric<int> multiplier = 1;
  if (is_canary_dev)
    multiplier += process_sampling_boost;
  if (is_browser_process)
    multiplier += process_sampling_boost;

  if (!multiplier.IsValid() || multiplier.ValueOrDie() < 1) {
    DLOG(ERROR) << "GWP-ASan ProcessSampling multiplier is out-of-range";
    return false;
  }

  base::CheckedNumeric<double> sampling_prob_mult =
      process_sampling_probability;
  sampling_prob_mult *= multiplier.ValueOrDie();
  if (!sampling_prob_mult.IsValid()) {
    DLOG(ERROR) << "GWP-ASan multiplier caused out-of-range multiply: "
                << multiplier.ValueOrDie();
    return false;
  }

  process_sampling_probability = sampling_prob_mult.ValueOrDie();
  return (base::RandDouble() < process_sampling_probability);
}

// Returns the allocation sampling frequency, or 0 on error.
size_t AllocationSamplingFrequency(const base::Feature& feature) {
  int multiplier =
      GetFieldTrialParamByFeatureAsInt(feature, "AllocationSamplingMultiplier",
                                       kDefaultAllocationSamplingMultiplier);
  if (multiplier < 1) {
    DLOG(ERROR) << "GWP-ASan AllocationSamplingMultiplier is out-of-range: "
                << multiplier;
    return 0;
  }

  int range = GetFieldTrialParamByFeatureAsInt(
      feature, "AllocationSamplingRange", kDefaultAllocationSamplingRange);
  if (range < 1) {
    DLOG(ERROR) << "GWP-ASan AllocationSamplingRange is out-of-range: "
                << range;
    return 0;
  }

  base::CheckedNumeric<size_t> frequency = multiplier;
  frequency *= std::pow(range, base::RandDouble());
  if (!frequency.IsValid()) {
    DLOG(ERROR) << "Out-of-range multiply " << multiplier << " " << range;
    return 0;
  }

  return frequency.ValueOrDie();
}

}  // namespace

// Exported for testing.
GWP_ASAN_EXPORT base::Optional<AllocatorSettings> GetAllocatorSettings(
    const base::Feature& feature,
    bool is_canary_dev,
    bool is_browser_process) {
  if (!base::FeatureList::IsEnabled(feature))
    return base::nullopt;

  static_assert(AllocatorState::kMaxSlots <= std::numeric_limits<int>::max(),
                "kMaxSlots out of range");
  constexpr int kMaxSlots = static_cast<int>(AllocatorState::kMaxSlots);

  static_assert(AllocatorState::kMaxMetadata <= std::numeric_limits<int>::max(),
                "kMaxMetadata out of range");
  constexpr int kMaxMetadata = static_cast<int>(AllocatorState::kMaxMetadata);

  int total_pages = GetFieldTrialParamByFeatureAsInt(feature, "TotalPages",
                                                     kDefaultTotalPages);
  if (total_pages < 1 || total_pages > kMaxSlots) {
    DLOG(ERROR) << "GWP-ASan TotalPages is out-of-range: " << total_pages;
    return base::nullopt;
  }

  int max_metadata = GetFieldTrialParamByFeatureAsInt(feature, "MaxMetadata",
                                                      kDefaultMaxMetadata);
  if (max_metadata < 1 || max_metadata > std::min(total_pages, kMaxMetadata)) {
    DLOG(ERROR) << "GWP-ASan MaxMetadata is out-of-range: " << max_metadata
                << " with TotalPages = " << total_pages;
    return base::nullopt;
  }

  int max_allocations = GetFieldTrialParamByFeatureAsInt(
      feature, "MaxAllocations", kDefaultMaxAllocations);
  if (max_allocations < 1 || max_allocations > max_metadata) {
    DLOG(ERROR) << "GWP-ASan MaxAllocations is out-of-range: "
                << max_allocations << " with MaxMetadata = " << max_metadata;
    return base::nullopt;
  }

  size_t alloc_sampling_freq = AllocationSamplingFrequency(feature);
  if (!alloc_sampling_freq)
    return base::nullopt;

  if (!SampleProcess(feature, is_canary_dev, is_browser_process))
    return base::nullopt;

  return AllocatorSettings{max_allocations, max_metadata, total_pages,
                           alloc_sampling_freq};
}

}  // namespace internal

void EnableForMalloc(bool is_canary_dev, bool is_browser_process) {
#if BUILDFLAG(USE_ALLOCATOR_SHIM)
  static bool init_once = [&]() -> bool {
    auto settings = internal::GetAllocatorSettings(
        internal::kGwpAsan, is_canary_dev, is_browser_process);
    if (!settings)
      return false;

    internal::InstallMallocHooks(settings->max_allocated_pages,
                                 settings->num_metadata, settings->total_pages,
                                 settings->sampling_frequency);
    return true;
  }();
  ignore_result(init_once);
#else
  ignore_result(internal::kGwpAsan);
  DLOG(WARNING) << "base::allocator shims are unavailable for GWP-ASan.";
#endif  // BUILDFLAG(USE_ALLOCATOR_SHIM)
}

}  // namespace gwp_asan
