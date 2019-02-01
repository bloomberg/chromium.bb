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

const base::Feature kGwpAsan{"GwpAsanMalloc",
                             base::FEATURE_DISABLED_BY_DEFAULT};

const base::FeatureParam<int> kMaxAllocationsParam{&kGwpAsan, "MaxAllocations",
                                                   7};

const base::FeatureParam<int> kTotalPagesParam{&kGwpAsan, "TotalPages", 30};

const base::FeatureParam<int> kAllocationSamplingParam{
    &kGwpAsan, "AllocationSamplingFrequency", 1000};

const base::FeatureParam<double> kProcessSamplingParam{
    &kGwpAsan, "ProcessSamplingProbability", 1.0};

// The multiplier to increase MaxAllocations/TotalPages on canary/dev builds.
const base::FeatureParam<int> kCanaryDevMultiplierParam{
    &kGwpAsan, "CanaryDevMultiplier", 5};

bool EnableForMalloc(bool is_canary_dev) {
  if (!base::FeatureList::IsEnabled(kGwpAsan))
    return false;

  static_assert(AllocatorState::kGpaMaxPages <= std::numeric_limits<int>::max(),
                "kGpaMaxPages out of range");
  constexpr int kMaxPages = static_cast<int>(AllocatorState::kGpaMaxPages);

  int total_pages = kTotalPagesParam.Get();
  if (total_pages < 1 || total_pages > kMaxPages) {
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

  int multiplier = 1;
  if (is_canary_dev)
    multiplier = kCanaryDevMultiplierParam.Get();

  if (multiplier < 1 || multiplier > kMaxPages) {
    DLOG(ERROR) << "GWP-ASan CanaryDevMultiplier is out-of-range: "
                << multiplier;
    return false;
  }

  base::CheckedNumeric<int> total_pages_mult = total_pages;
  total_pages_mult *= multiplier;
  base::CheckedNumeric<int> max_allocations_mult = max_allocations;
  max_allocations_mult *= multiplier;

  if (!total_pages_mult.IsValid() || !max_allocations_mult.IsValid()) {
    DLOG(ERROR) << "GWP-ASan multiplier caused out-of-range multiply: "
                << multiplier;
    return false;
  }

  total_pages = std::min<int>(total_pages_mult.ValueOrDie(), kMaxPages);
  max_allocations =
      std::min<int>(max_allocations_mult.ValueOrDie(), total_pages);

  if (base::RandDouble() >= process_sampling_probability)
    return false;

  InstallAllocatorHooks(max_allocations, total_pages, alloc_sampling_freq);
  return true;
}

}  // namespace
}  // namespace internal

void EnableForMalloc(bool is_canary_dev) {
  static bool init_once = internal::EnableForMalloc(is_canary_dev);
  ignore_result(init_once);
}

}  // namespace gwp_asan
