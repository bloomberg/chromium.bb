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

#if BUILDFLAG(USE_ALLOCATOR_SHIM)
constexpr int kDefaultMaxAllocations = 35;
constexpr int kDefaultMaxMetadata = 150;

#if defined(ARCH_CPU_64_BITS)
constexpr int kDefaultTotalPages = 2048;
#else
// Use much less virtual memory on 32-bit builds (where OOMing due to lack of
// address space is a concern.)
constexpr int kDefaultTotalPages = kDefaultMaxMetadata * 2;
#endif

constexpr int kDefaultAllocationSamplingMultiplier = 1000;
constexpr int kDefaultAllocationSamplingRange = 64;
constexpr double kDefaultProcessSamplingProbability = 0.2;
constexpr int kDefaultProcessSamplingBoost = 4;

const base::Feature kGwpAsan{"GwpAsanMalloc", base::FEATURE_ENABLED_BY_DEFAULT};

const base::FeatureParam<int> kMaxAllocationsParam{&kGwpAsan, "MaxAllocations",
                                                   kDefaultMaxAllocations};

const base::FeatureParam<int> kMaxMetadataParam{&kGwpAsan, "MaxMetadata",
                                                kDefaultMaxMetadata};

const base::FeatureParam<int> kTotalPagesParam{&kGwpAsan, "TotalPages",
                                               kDefaultTotalPages};

// The allocation sampling frequency is calculated using the formula:
// multiplier * range**rand
// where rand is a random real number in the range [0,1).
const base::FeatureParam<int> kAllocationSamplingMultiplierParam{
    &kGwpAsan, "AllocationSamplingMultiplier",
    kDefaultAllocationSamplingMultiplier};

const base::FeatureParam<int> kAllocationSamplingRangeParam{
    &kGwpAsan, "AllocationSamplingRange", kDefaultAllocationSamplingRange};

const base::FeatureParam<double> kProcessSamplingParam{
    &kGwpAsan, "ProcessSamplingProbability",
    kDefaultProcessSamplingProbability};

// The multiplier to increase the ProcessSamplingProbability in scenarios where
// we want to perform additional testing (e.g. on canary/dev builds or in the
// browser process.) The multiplier increase is cumulative when multiple
// conditions apply.
const base::FeatureParam<int> kProcessSamplingBoostParam{
    &kGwpAsan, "ProcessSamplingBoost", kDefaultProcessSamplingBoost};

// Returns whether this process should be sampled to enable GWP-ASan.
bool SampleProcess(bool is_canary_dev, bool is_browser_process) {
  double process_sampling_probability = kProcessSamplingParam.Get();
  if (process_sampling_probability < 0.0 ||
      process_sampling_probability > 1.0) {
    DLOG(ERROR) << "GWP-ASan ProcessSamplingProbability is out-of-range: "
                << process_sampling_probability;
    return false;
  }

  base::CheckedNumeric<int> multiplier = 1;
  if (is_canary_dev)
    multiplier += kProcessSamplingBoostParam.Get();
  if (is_browser_process)
    multiplier += kProcessSamplingBoostParam.Get();

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
size_t AllocationSamplingFrequency() {
  int multiplier = kAllocationSamplingMultiplierParam.Get();
  if (multiplier < 1) {
    DLOG(ERROR) << "GWP-ASan AllocationSamplingMultiplier is out-of-range: "
                << multiplier;
    return 0;
  }

  int range = kAllocationSamplingRangeParam.Get();
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

bool EnableForMalloc(bool is_canary_dev, bool is_browser_process) {
  if (!base::FeatureList::IsEnabled(kGwpAsan))
    return false;

  static_assert(AllocatorState::kMaxSlots <= std::numeric_limits<int>::max(),
                "kMaxSlots out of range");
  constexpr int kMaxSlots = static_cast<int>(AllocatorState::kMaxSlots);

  static_assert(AllocatorState::kMaxMetadata <= std::numeric_limits<int>::max(),
                "kMaxMetadata out of range");
  constexpr int kMaxMetadata = static_cast<int>(AllocatorState::kMaxMetadata);

  int total_pages = kTotalPagesParam.Get();
  if (total_pages < 1 || total_pages > kMaxSlots) {
    DLOG(ERROR) << "GWP-ASan TotalPages is out-of-range: " << total_pages;
    return false;
  }

  int max_metadata = kMaxMetadataParam.Get();
  if (max_metadata < 1 || max_metadata > std::min(total_pages, kMaxMetadata)) {
    DLOG(ERROR) << "GWP-ASan MaxMetadata is out-of-range: " << max_metadata
                << " with TotalPages = " << total_pages;
    return false;
  }

  int max_allocations = kMaxAllocationsParam.Get();
  if (max_allocations < 1 || max_allocations > max_metadata) {
    DLOG(ERROR) << "GWP-ASan MaxAllocations is out-of-range: "
                << max_allocations << " with MaxMetadata = " << max_metadata;
    return false;
  }

  size_t alloc_sampling_freq = AllocationSamplingFrequency();
  if (!alloc_sampling_freq)
    return false;

  if (!SampleProcess(is_canary_dev, is_browser_process))
    return false;

  InstallMallocHooks(max_allocations, max_metadata, total_pages,
                     alloc_sampling_freq);
  return true;
}
#endif  // BUILDFLAG(USE_ALLOCATOR_SHIM)

}  // namespace
}  // namespace internal

void EnableForMalloc(bool is_canary_dev, bool is_browser_process) {
#if BUILDFLAG(USE_ALLOCATOR_SHIM)
  static bool init_once =
      internal::EnableForMalloc(is_canary_dev, is_browser_process);
  ignore_result(init_once);
#else
  DLOG(WARNING) << "base::allocator shims are unavailable for GWP-ASan.";
#endif  // BUILDFLAG(USE_ALLOCATOR_SHIM)
}

}  // namespace gwp_asan
