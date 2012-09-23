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

using content::GpuFeatureType;
using content::GpuSwitchingOption;

namespace {

const char kGpuFeatureNameAccelerated2dCanvas[] = "accelerated_2d_canvas";
const char kGpuFeatureNameAcceleratedCompositing[] = "accelerated_compositing";
const char kGpuFeatureNameWebgl[] = "webgl";
const char kGpuFeatureNameMultisampling[] = "multisampling";
const char kGpuFeatureNameFlash3d[] = "flash_3d";
const char kGpuFeatureNameFlashStage3d[] = "flash_stage3d";
const char kGpuFeatureNameTextureSharing[] = "texture_sharing";
const char kGpuFeatureNameAcceleratedVideoDecode[] = "accelerated_video_decode";
const char kGpuFeatureNameAll[] = "all";
const char kGpuFeatureNameUnknown[] = "unknown";

const char kGpuSwitchingNameAutomatic[] = "automatic";
const char kGpuSwitchingNameForceIntegrated[] = "force_integrated";
const char kGpuSwitchingNameForceDiscrete[] = "force_discrete";
const char kGpuSwitchingNameUnknown[] = "unknown";

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

}  // namespace

namespace gpu_util {

GpuFeatureType StringToGpuFeatureType(const std::string& feature_string) {
  if (feature_string == kGpuFeatureNameAccelerated2dCanvas)
    return content::GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS;
  if (feature_string == kGpuFeatureNameAcceleratedCompositing)
    return content::GPU_FEATURE_TYPE_ACCELERATED_COMPOSITING;
  if (feature_string == kGpuFeatureNameWebgl)
    return content::GPU_FEATURE_TYPE_WEBGL;
  if (feature_string == kGpuFeatureNameMultisampling)
    return content::GPU_FEATURE_TYPE_MULTISAMPLING;
  if (feature_string == kGpuFeatureNameFlash3d)
    return content::GPU_FEATURE_TYPE_FLASH3D;
  if (feature_string == kGpuFeatureNameFlashStage3d)
    return content::GPU_FEATURE_TYPE_FLASH_STAGE3D;
  if (feature_string == kGpuFeatureNameTextureSharing)
    return content::GPU_FEATURE_TYPE_TEXTURE_SHARING;
  if (feature_string == kGpuFeatureNameAcceleratedVideoDecode)
    return content::GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE;
  if (feature_string == kGpuFeatureNameAll)
    return content::GPU_FEATURE_TYPE_ALL;
  return content::GPU_FEATURE_TYPE_UNKNOWN;
}

std::string GpuFeatureTypeToString(GpuFeatureType type) {
  std::vector<std::string> matches;
  if (type == content::GPU_FEATURE_TYPE_ALL) {
    matches.push_back(kGpuFeatureNameAll);
  } else {
    if (type & content::GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS)
      matches.push_back(kGpuFeatureNameAccelerated2dCanvas);
    if (type & content::GPU_FEATURE_TYPE_ACCELERATED_COMPOSITING)
      matches.push_back(kGpuFeatureNameAcceleratedCompositing);
    if (type & content::GPU_FEATURE_TYPE_WEBGL)
      matches.push_back(kGpuFeatureNameWebgl);
    if (type & content::GPU_FEATURE_TYPE_MULTISAMPLING)
      matches.push_back(kGpuFeatureNameMultisampling);
    if (type & content::GPU_FEATURE_TYPE_FLASH3D)
      matches.push_back(kGpuFeatureNameFlash3d);
    if (type & content::GPU_FEATURE_TYPE_FLASH_STAGE3D)
      matches.push_back(kGpuFeatureNameFlashStage3d);
    if (type & content::GPU_FEATURE_TYPE_TEXTURE_SHARING)
      matches.push_back(kGpuFeatureNameTextureSharing);
    if (type & content::GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE)
      matches.push_back(kGpuFeatureNameAcceleratedVideoDecode);
    if (!matches.size())
      matches.push_back(kGpuFeatureNameUnknown);
  }
  return JoinString(matches, ',');
}

GpuSwitchingOption StringToGpuSwitchingOption(
    const std::string& switching_string) {
  if (switching_string == kGpuSwitchingNameAutomatic)
    return content::GPU_SWITCHING_AUTOMATIC;
  if (switching_string == kGpuSwitchingNameForceIntegrated)
    return content::GPU_SWITCHING_FORCE_INTEGRATED;
  if (switching_string == kGpuSwitchingNameForceDiscrete)
    return content::GPU_SWITCHING_FORCE_DISCRETE;
  return content::GPU_SWITCHING_UNKNOWN;
}

void UpdateStats(const GpuBlacklist* blacklist,
                 uint32 blacklisted_features) {
  uint32 max_entry_id = blacklist->max_entry_id();
  if (max_entry_id == 0) {
    // GPU Blacklist was not loaded.  No need to go further.
    return;
  }

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  bool disabled = false;
  if (blacklisted_features == 0) {
    UMA_HISTOGRAM_ENUMERATION("GPU.BlacklistTestResultsPerEntry",
        0, max_entry_id + 1);
  } else {
    std::vector<uint32> flag_entries;
    blacklist->GetDecisionEntries(flag_entries, disabled);
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
  blacklist->GetDecisionEntries(flag_disabled_entries, disabled);
  for (size_t i = 0; i < flag_disabled_entries.size(); ++i) {
    UMA_HISTOGRAM_ENUMERATION("GPU.BlacklistTestResultsPerDisabledEntry",
        flag_disabled_entries[i], max_entry_id + 1);
  }

  const content::GpuFeatureType kGpuFeatures[] = {
      content::GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS,
      content::GPU_FEATURE_TYPE_ACCELERATED_COMPOSITING,
      content::GPU_FEATURE_TYPE_WEBGL
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
      sizeof(kGpuFeatures) / sizeof(content::GpuFeatureType);
  for (size_t i = 0; i < kNumFeatures; ++i) {
    // We can't use UMA_HISTOGRAM_ENUMERATION here because the same name is
    // expected if the macro is used within a loop.
    GpuFeatureStatus value = kGpuFeatureEnabled;
    if (blacklisted_features & kGpuFeatures[i])
      value = kGpuFeatureBlacklisted;
    else if (kGpuFeatureUserFlags[i])
      value = kGpuFeatureDisabled;
    base::Histogram* histogram_pointer = base::LinearHistogram::FactoryGet(
        kGpuBlacklistFeatureHistogramNames[i],
        1, kGpuFeatureNumStatus, kGpuFeatureNumStatus + 1,
        base::Histogram::kUmaTargetedHistogramFlag);
    histogram_pointer->Add(value);
#if defined(OS_WIN)
    histogram_pointer = base::LinearHistogram::FactoryGet(
        kGpuBlacklistFeatureHistogramNamesWin[i],
        1, kNumWinSubVersions * kGpuFeatureNumStatus,
        kNumWinSubVersions * kGpuFeatureNumStatus + 1,
        base::Histogram::kUmaTargetedHistogramFlag);
    histogram_pointer->Add(GetGpuBlacklistHistogramValueWin(value));
#endif
  }
}

}  // namespace gpu_util;
