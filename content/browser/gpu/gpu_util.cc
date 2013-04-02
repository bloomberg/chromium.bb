// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_util.h"

#include <vector>

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/string_util.h"
#include "base/sys_info.h"
#include "base/version.h"
#include "content/browser/gpu/gpu_blacklist.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/gpu_feature_type.h"
#include "ui/gl/gl_switches.h"

namespace content {
namespace {

enum GpuFeatureStatus {
    kGpuFeatureEnabled = 0,
    kGpuFeatureBlacklisted = 1,
    kGpuFeatureDisabled = 2,  // disabled by user but not blacklisted
    kGpuFeatureNumStatus
};

#if defined(OS_WIN)

enum WinSubVersion {
  kWinOthers = 0,
  kWinXP,
  kWinVista,
  kWin7,
  kNumWinSubVersions
};

int GetGpuBlacklistHistogramValueWin(GpuFeatureStatus status) {
  static WinSubVersion sub_version = kNumWinSubVersions;
  if (sub_version == kNumWinSubVersions) {
    sub_version = kWinOthers;
    std::string version_str = base::SysInfo::OperatingSystemVersion();
    size_t pos = version_str.find_first_not_of("0123456789.");
    if (pos != std::string::npos)
      version_str = version_str.substr(0, pos);
    Version os_version(version_str);
    if (os_version.IsValid() && os_version.components().size() >= 2) {
      const std::vector<uint16>& version_numbers = os_version.components();
      if (version_numbers[0] == 5)
        sub_version = kWinXP;
      else if (version_numbers[0] == 6 && version_numbers[1] == 0)
        sub_version = kWinVista;
      else if (version_numbers[0] == 6 && version_numbers[1] == 1)
        sub_version = kWin7;
    }
  }
  int entry_index = static_cast<int>(sub_version) * kGpuFeatureNumStatus;
  switch (status) {
    case kGpuFeatureEnabled:
      break;
    case kGpuFeatureBlacklisted:
      entry_index++;
      break;
    case kGpuFeatureDisabled:
      entry_index += 2;
      break;
  }
  return entry_index;
}
#endif  // OS_WIN

}  // namespace anonymous

GpuSwitchingOption StringToGpuSwitchingOption(
    const std::string& switching_string) {
  if (switching_string == switches::kGpuSwitchingOptionNameAutomatic)
    return GPU_SWITCHING_OPTION_AUTOMATIC;
  if (switching_string == switches::kGpuSwitchingOptionNameForceIntegrated)
    return GPU_SWITCHING_OPTION_FORCE_INTEGRATED;
  if (switching_string == switches::kGpuSwitchingOptionNameForceDiscrete)
    return GPU_SWITCHING_OPTION_FORCE_DISCRETE;
  return GPU_SWITCHING_OPTION_UNKNOWN;
}

std::string GpuSwitchingOptionToString(GpuSwitchingOption option) {
  switch (option) {
    case GPU_SWITCHING_OPTION_AUTOMATIC:
      return switches::kGpuSwitchingOptionNameAutomatic;
    case GPU_SWITCHING_OPTION_FORCE_INTEGRATED:
      return switches::kGpuSwitchingOptionNameForceIntegrated;
    case GPU_SWITCHING_OPTION_FORCE_DISCRETE:
      return switches::kGpuSwitchingOptionNameForceDiscrete;
    default:
      return "unknown";
  }
}

void UpdateStats(const GpuBlacklist* blacklist,
                 const std::set<int>& blacklisted_features) {
  uint32 max_entry_id = blacklist->max_entry_id();
  if (max_entry_id == 0) {
    // GPU Blacklist was not loaded.  No need to go further.
    return;
  }

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  bool disabled = false;
  if (blacklisted_features.size() == 0) {
    UMA_HISTOGRAM_ENUMERATION("GPU.BlacklistTestResultsPerEntry",
        0, max_entry_id + 1);
  } else {
    std::vector<uint32> flag_entries;
    blacklist->GetDecisionEntries(&flag_entries, disabled);
    DCHECK_GT(flag_entries.size(), 0u);
    for (size_t i = 0; i < flag_entries.size(); ++i) {
      UMA_HISTOGRAM_ENUMERATION("GPU.BlacklistTestResultsPerEntry",
          flag_entries[i], max_entry_id + 1);
    }
  }

  // This counts how many users are affected by a disabled entry - this allows
  // us to understand the impact of an entry before enable it.
  std::vector<uint32> flag_disabled_entries;
  disabled = true;
  blacklist->GetDecisionEntries(&flag_disabled_entries, disabled);
  for (size_t i = 0; i < flag_disabled_entries.size(); ++i) {
    UMA_HISTOGRAM_ENUMERATION("GPU.BlacklistTestResultsPerDisabledEntry",
        flag_disabled_entries[i], max_entry_id + 1);
  }

  const GpuFeatureType kGpuFeatures[] = {
      GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS,
      GPU_FEATURE_TYPE_ACCELERATED_COMPOSITING,
      GPU_FEATURE_TYPE_WEBGL
  };
  const std::string kGpuBlacklistFeatureHistogramNames[] = {
      "GPU.BlacklistFeatureTestResults.Accelerated2dCanvas",
      "GPU.BlacklistFeatureTestResults.AcceleratedCompositing",
      "GPU.BlacklistFeatureTestResults.Webgl"
  };
  const bool kGpuFeatureUserFlags[] = {
      command_line.HasSwitch(switches::kDisableAccelerated2dCanvas),
      command_line.HasSwitch(switches::kDisableAcceleratedCompositing),
#if defined(OS_ANDROID)
      !command_line.HasSwitch(switches::kEnableExperimentalWebGL)
#else
      command_line.HasSwitch(switches::kDisableExperimentalWebGL)
#endif
  };
#if defined(OS_WIN)
  const std::string kGpuBlacklistFeatureHistogramNamesWin[] = {
      "GPU.BlacklistFeatureTestResultsWindows.Accelerated2dCanvas",
      "GPU.BlacklistFeatureTestResultsWindows.AcceleratedCompositing",
      "GPU.BlacklistFeatureTestResultsWindows.Webgl"
  };
#endif
  const size_t kNumFeatures =
      sizeof(kGpuFeatures) / sizeof(GpuFeatureType);
  for (size_t i = 0; i < kNumFeatures; ++i) {
    // We can't use UMA_HISTOGRAM_ENUMERATION here because the same name is
    // expected if the macro is used within a loop.
    GpuFeatureStatus value = kGpuFeatureEnabled;
    if (blacklisted_features.count(kGpuFeatures[i]))
      value = kGpuFeatureBlacklisted;
    else if (kGpuFeatureUserFlags[i])
      value = kGpuFeatureDisabled;
    base::HistogramBase* histogram_pointer = base::LinearHistogram::FactoryGet(
        kGpuBlacklistFeatureHistogramNames[i],
        1, kGpuFeatureNumStatus, kGpuFeatureNumStatus + 1,
        base::HistogramBase::kUmaTargetedHistogramFlag);
    histogram_pointer->Add(value);
#if defined(OS_WIN)
    histogram_pointer = base::LinearHistogram::FactoryGet(
        kGpuBlacklistFeatureHistogramNamesWin[i],
        1, kNumWinSubVersions * kGpuFeatureNumStatus,
        kNumWinSubVersions * kGpuFeatureNumStatus + 1,
        base::HistogramBase::kUmaTargetedHistogramFlag);
    histogram_pointer->Add(GetGpuBlacklistHistogramValueWin(value));
#endif
  }
}

void MergeFeatureSets(std::set<int>* dst, const std::set<int>& src) {
  DCHECK(dst);
  dst->insert(src.begin(), src.end());
}

}  // namespace content
