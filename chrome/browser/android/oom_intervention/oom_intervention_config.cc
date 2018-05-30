// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/oom_intervention/oom_intervention_config.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_info.h"
#include "chrome/common/chrome_features.h"

namespace {

const char kUseComponentCallbacks[] = "use_component_callbacks";
const char kSwapFreeThresholdRatioParamName[] = "swap_free_threshold_ratio";
const char kRendererWorkloadThresholdDeprecated[] =
    "renderer_workload_threshold";
const char kRendererWorkloadThresholdPercentage[] =
    "renderer_workload_threshold_percentage";

// Default SwapFree/SwapTotal ratio for detecting near-OOM situation.
// TODO(bashi): Confirm that this is appropriate.
const double kDefaultSwapFreeThresholdRatio = 0.45;

// Field trial parameter names.
const char kRendererPauseParamName[] = "pause_renderer";
const char kShouldDetectInRenderer[] = "detect_in_renderer";

bool GetThresholdParam(const char* param,
                       size_t ram_size,
                       uint64_t* threshold) {
  std::string str =
      base::GetFieldTrialParamValueByFeature(features::kOomIntervention, param);
  uint64_t value = 0;
  if (!base::StringToUint64(str, &value))
    return false;
  *threshold = value * ram_size / 100;
  return true;
}

bool GetRendererMemoryThreshold(uint64_t* threshold) {
  const uint64_t kDefaultMemoryWorkloadThresholdPercent = 16;
  static size_t kRAMSize = base::SysInfo::AmountOfPhysicalMemory();
  *threshold = 0;

  if (GetThresholdParam(kRendererWorkloadThresholdPercentage, kRAMSize,
                        threshold)) {
    return true;
  }

  // If the old trigger param is set, then enable intervention only on 512MB
  // devices.
  if (kRAMSize > 512 * 1024 * 1024)
    return false;

  std::string threshold_str = base::GetFieldTrialParamValueByFeature(
      features::kOomIntervention, kRendererWorkloadThresholdDeprecated);
  if (base::StringToUint64(threshold_str, threshold))
    return true;

  *threshold = kDefaultMemoryWorkloadThresholdPercent * kRAMSize / 100;
  return true;
}

bool GetSwapFreeThreshold(uint64_t* threshold) {
  base::SystemMemoryInfoKB memory_info;
  if (!base::GetSystemMemoryInfo(&memory_info))
    return false;

  // If there is no swap (zram) the monitor doesn't work because we use
  // SwapFree as the tracking metric.
  if (memory_info.swap_total == 0)
    return false;

  double threshold_ratio = base::GetFieldTrialParamByFeatureAsDouble(
      features::kOomIntervention, kSwapFreeThresholdRatioParamName,
      kDefaultSwapFreeThresholdRatio);
  *threshold = static_cast<uint64_t>(memory_info.swap_total * threshold_ratio);
  return true;
}

}  // namespace

OomInterventionConfig::OomInterventionConfig()
    : is_intervention_enabled_(
          base::FeatureList::IsEnabled(features::kOomIntervention)) {
  if (!is_intervention_enabled_)
    return;

  is_renderer_pause_enabled_ = base::GetFieldTrialParamByFeatureAsBool(
      features::kOomIntervention, kRendererPauseParamName, false);
  should_detect_in_renderer_ = base::GetFieldTrialParamByFeatureAsBool(
      features::kOomIntervention, kShouldDetectInRenderer, true);

  use_components_callback_ = base::GetFieldTrialParamByFeatureAsBool(
      features::kOomIntervention, kUseComponentCallbacks, true);

  // Enable intervention only if at least one threshold is set for detection
  // in each process.
  if (!GetSwapFreeThreshold(&swapfree_threshold_) ||
      !GetRendererMemoryThreshold(&renderer_workload_threshold_)) {
    is_intervention_enabled_ = false;
  }
}

// static
const OomInterventionConfig* OomInterventionConfig::GetInstance() {
  static OomInterventionConfig* config = new OomInterventionConfig();
  return config;
}
