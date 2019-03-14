// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/gwp_asan/client/gwp_asan.h"

#include <algorithm>
#include <limits>

#include "base/debug/crash_logging.h"
#include "base/feature_list.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/metrics/field_trial_params.h"
#include "base/numerics/safe_math.h"
#include "base/rand_util.h"
#include "base/strings/stringprintf.h"
#include "components/gwp_asan/client/guarded_page_allocator.h"
#include "components/gwp_asan/client/sampling_allocator_shims.h"

namespace gwp_asan {

namespace internal {
namespace {

constexpr int kDefaultMaxAllocations = 7;
constexpr int kDefaultTotalPages = 30;
constexpr int kDefaultAllocationSamplingFrequency = 1000;
constexpr double kDefaultProcessSamplingProbability = 1.0;
constexpr int kDefaultIncreasedMemoryMultiplier = 4;

const base::Feature kGwpAsan{"GwpAsanMalloc",
                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::FeatureParam<int> kMaxAllocationsParam{&kGwpAsan, "MaxAllocations",
                                                   kDefaultMaxAllocations};

const base::FeatureParam<int> kTotalPagesParam{&kGwpAsan, "TotalPages",
                                               kDefaultTotalPages};

const base::FeatureParam<int> kAllocationSamplingParam{
    &kGwpAsan, "AllocationSamplingFrequency",
    kDefaultAllocationSamplingFrequency};

const base::FeatureParam<double> kProcessSamplingParam{
    &kGwpAsan, "ProcessSamplingProbability",
    kDefaultProcessSamplingProbability};

// The multiplier to increase MaxAllocations/TotalPages in scenarios where we
// want to perform additional testing (e.g. on canary/dev builds or in the
// browser process.) The multiplier increase is cumulative when multiple
// conditions apply.
const base::FeatureParam<int> kIncreasedMemoryMultiplierParam{
    &kGwpAsan, "IncreasedMemoryMultiplier", kDefaultIncreasedMemoryMultiplier};

bool EnableForMalloc(bool is_canary_dev, bool is_browser_process) {
  if (!base::FeatureList::IsEnabled(kGwpAsan))
    return false;

  static_assert(AllocatorState::kMaxSlots <= std::numeric_limits<int>::max(),
                "kMaxSlots out of range");
  constexpr int kMaxSlots = static_cast<int>(AllocatorState::kMaxSlots);

  int total_pages = kTotalPagesParam.Get();
  if (total_pages < 1 || total_pages > kMaxSlots) {
    DLOG(ERROR) << "GWP-ASan TotalPages is out-of-range: " << total_pages;
    return false;
  }

  int max_allocations = kMaxAllocationsParam.Get();
  if (max_allocations < 1 || max_allocations > total_pages) {
    DLOG(ERROR) << "GWP-ASan MaxAllocations is out-of-range: "
                << max_allocations << " with TotalPages = " << total_pages;
    return false;
  }

  int alloc_sampling_freq = kAllocationSamplingParam.Get();
  if (alloc_sampling_freq < 1) {
    DLOG(ERROR) << "GWP-ASan AllocationSamplingFrequency is out-of-range: "
                << alloc_sampling_freq;
    return false;
  }

  double process_sampling_probability = kProcessSamplingParam.Get();
  if (process_sampling_probability < 0.0 ||
      process_sampling_probability > 1.0) {
    DLOG(ERROR) << "GWP-ASan ProcessSamplingProbability is out-of-range: "
                << process_sampling_probability;
    return false;
  }

  base::CheckedNumeric<int> multiplier = 1;
  if (is_canary_dev)
    multiplier += kIncreasedMemoryMultiplierParam.Get();
  if (is_browser_process)
    multiplier += kIncreasedMemoryMultiplierParam.Get();

  if (!multiplier.IsValid() || multiplier.ValueOrDie() < 1 ||
      multiplier.ValueOrDie() > kMaxSlots) {
    DLOG(ERROR) << "GWP-ASan IncreaseMemoryMultiplier is out-of-range";
    return false;
  }

  base::CheckedNumeric<int> total_pages_mult = total_pages;
  total_pages_mult *= multiplier.ValueOrDie();
  base::CheckedNumeric<int> max_allocations_mult = max_allocations;
  max_allocations_mult *= multiplier.ValueOrDie();

  if (!total_pages_mult.IsValid() || !max_allocations_mult.IsValid()) {
    DLOG(ERROR) << "GWP-ASan multiplier caused out-of-range multiply: "
                << multiplier.ValueOrDie();
    return false;
  }

  total_pages = std::min<int>(total_pages_mult.ValueOrDie(), kMaxSlots);
  max_allocations =
      std::min<int>(max_allocations_mult.ValueOrDie(), total_pages);

  if (base::RandDouble() >= process_sampling_probability)
    return false;

  InstallAllocatorHooks(max_allocations, total_pages, total_pages,
                        alloc_sampling_freq);
  return true;
}

}  // namespace
}  // namespace internal

void EnableForMalloc(bool is_canary_dev, bool is_browser_process) {
  static bool init_once =
      internal::EnableForMalloc(is_canary_dev, is_browser_process);
  ignore_result(init_once);
}

}  // namespace gwp_asan
