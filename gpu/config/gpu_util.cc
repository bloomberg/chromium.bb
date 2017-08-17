// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_util.h"

#include <memory>
#include <vector>

#include "base/command_line.h"
#include "base/debug/crash_logging.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/sys_info.h"
#include "gpu/config/gpu_blacklist.h"
#include "gpu/config/gpu_crash_keys.h"
#include "gpu/config/gpu_driver_bug_list.h"
#include "gpu/config/gpu_driver_bug_workaround_type.h"
#include "gpu/config/gpu_feature_type.h"
#include "gpu/config/gpu_finch_features.h"
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_switches.h"
#include "ui/gl/gl_switches.h"
#include "ui/gl/gpu_switching_manager.h"

namespace gpu {

namespace {

void StringToIntSet(const std::string& str, std::set<int>* list) {
  DCHECK(list);
  for (const base::StringPiece& piece :
       base::SplitStringPiece(str, ",", base::TRIM_WHITESPACE,
                              base::SPLIT_WANT_ALL)) {
    int number = 0;
    bool succeed = base::StringToInt(piece, &number);
    DCHECK(succeed);
    list->insert(number);
  }
}

// |str| is in the format of "0x040a;0x10de;...;hex32_N".
void StringToIds(const std::string& str, std::vector<uint32_t>* list) {
  DCHECK(list);
  for (const base::StringPiece& piece : base::SplitStringPiece(
           str, ";", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL)) {
    uint32_t id = 0;
    bool succeed = base::HexStringToUInt(piece, &id);
    DCHECK(succeed);
    list->push_back(id);
  }
}

GpuFeatureStatus GetGpuRasterizationFeatureStatus(
    const std::set<int>& blacklisted_features,
    const base::CommandLine& command_line) {
  if (command_line.HasSwitch(switches::kDisableGpuRasterization))
    return kGpuFeatureStatusDisabled;
  else if (command_line.HasSwitch(switches::kEnableGpuRasterization))
    return kGpuFeatureStatusEnabled;

  if (blacklisted_features.count(GPU_FEATURE_TYPE_GPU_RASTERIZATION))
    return kGpuFeatureStatusBlacklisted;

#if defined(OS_ANDROID)
  // GPU Raster is always enabled on non-low-end Android. On low-end, it is
  // controlled by a Finch experiment.
  if (!base::SysInfo::IsLowEndDevice())
    return kGpuFeatureStatusEnabled;
#endif  // defined(OS_ANDROID)

  // Gpu Rasterization on platforms that are not fully enabled is controlled by
  // a finch experiment.
  if (!base::FeatureList::IsEnabled(features::kDefaultEnableGpuRasterization))
    return kGpuFeatureStatusDisabled;

  return kGpuFeatureStatusEnabled;
}

}  // namespace anonymous

std::string IntSetToString(const std::set<int>& list, char divider) {
  std::string rt;
  for (auto number : list) {
    if (!rt.empty())
      rt += divider;
    rt += base::IntToString(number);
  }
  return rt;
}

void ApplyGpuDriverBugWorkarounds(const GPUInfo& gpu_info,
                                  base::CommandLine* command_line) {
  std::unique_ptr<GpuDriverBugList> list(GpuDriverBugList::Create());
  std::set<int> workarounds = list->MakeDecision(
      GpuControlList::kOsAny, std::string(), gpu_info);
  GpuDriverBugList::AppendWorkaroundsFromCommandLine(
      &workarounds, *command_line);
  if (!workarounds.empty()) {
    command_line->AppendSwitchASCII(switches::kGpuDriverBugWorkarounds,
                                    IntSetToString(workarounds, ','));
  }

  std::vector<std::string> buglist_disabled_extensions =
      list->GetDisabledExtensions();
  std::set<base::StringPiece> disabled_extensions(
      buglist_disabled_extensions.begin(), buglist_disabled_extensions.end());

  // Must be outside if statement to remain in scope (referenced by
  // |disabled_extensions|).
  std::string command_line_disable_gl_extensions;
  if (command_line->HasSwitch(switches::kDisableGLExtensions)) {
    command_line_disable_gl_extensions =
        command_line->GetSwitchValueASCII(switches::kDisableGLExtensions);
    std::vector<base::StringPiece> existing_disabled_extensions =
        base::SplitStringPiece(command_line_disable_gl_extensions, " ",
                               base::TRIM_WHITESPACE,
                               base::SPLIT_WANT_NONEMPTY);
    disabled_extensions.insert(existing_disabled_extensions.begin(),
                               existing_disabled_extensions.end());
  }

  if (!disabled_extensions.empty()) {
    std::vector<base::StringPiece> v(disabled_extensions.begin(),
                                     disabled_extensions.end());
    command_line->AppendSwitchASCII(switches::kDisableGLExtensions,
                                    base::JoinString(v, " "));
  }
}

void StringToFeatureSet(
    const std::string& str, std::set<int>* feature_set) {
  StringToIntSet(str, feature_set);
}

void ParseSecondaryGpuDevicesFromCommandLine(
    const base::CommandLine& command_line,
    GPUInfo* gpu_info) {
  DCHECK(gpu_info);

  const char* secondary_vendor_switch_key = switches::kGpuSecondaryVendorIDs;
  const char* secondary_device_switch_key = switches::kGpuSecondaryDeviceIDs;

  if (command_line.HasSwitch(switches::kGpuTestingSecondaryVendorIDs) &&
      command_line.HasSwitch(switches::kGpuTestingSecondaryDeviceIDs)) {
    secondary_vendor_switch_key = switches::kGpuTestingSecondaryVendorIDs;
    secondary_device_switch_key = switches::kGpuTestingSecondaryDeviceIDs;
  }

  if (!command_line.HasSwitch(secondary_vendor_switch_key) ||
      !command_line.HasSwitch(secondary_device_switch_key)) {
    return;
  }

  std::vector<uint32_t> vendor_ids;
  std::vector<uint32_t> device_ids;
  StringToIds(command_line.GetSwitchValueASCII(secondary_vendor_switch_key),
              &vendor_ids);
  StringToIds(command_line.GetSwitchValueASCII(secondary_device_switch_key),
              &device_ids);

  DCHECK(vendor_ids.size() == device_ids.size());
  gpu_info->secondary_gpus.clear();
  for (size_t i = 0; i < vendor_ids.size() && i < device_ids.size(); ++i) {
    gpu::GPUInfo::GPUDevice secondary_device;
    secondary_device.active = false;
    secondary_device.vendor_id = vendor_ids[i];
    secondary_device.device_id = device_ids[i];
    gpu_info->secondary_gpus.push_back(secondary_device);
  }
}

void InitializeDualGpusIfSupported(
    const std::set<int>& driver_bug_workarounds) {
  ui::GpuSwitchingManager* switching_manager =
      ui::GpuSwitchingManager::GetInstance();
  if (!switching_manager->SupportsDualGpus())
    return;
  if (driver_bug_workarounds.count(gpu::FORCE_DISCRETE_GPU) == 1)
    ui::GpuSwitchingManager::GetInstance()->ForceUseOfDiscreteGpu();
  else if (driver_bug_workarounds.count(gpu::FORCE_INTEGRATED_GPU) == 1)
    ui::GpuSwitchingManager::GetInstance()->ForceUseOfIntegratedGpu();
}

GpuFeatureInfo GetGpuFeatureInfo(const GPUInfo& gpu_info,
                                 const base::CommandLine& command_line) {
  GpuFeatureInfo gpu_feature_info;
  std::set<int> blacklisted_features;
  if (!command_line.HasSwitch(switches::kIgnoreGpuBlacklist)) {
    std::unique_ptr<GpuBlacklist> list(GpuBlacklist::Create());
    blacklisted_features =
        list->MakeDecision(GpuControlList::kOsAny, std::string(), gpu_info);
  }

  // Currently only used for GPU rasterization.
  gpu_feature_info.status_values[GPU_FEATURE_TYPE_GPU_RASTERIZATION] =
      GetGpuRasterizationFeatureStatus(blacklisted_features, command_line);

  std::set<base::StringPiece> all_disabled_extensions;
  std::string disabled_gl_extensions_value =
      command_line.GetSwitchValueASCII(switches::kDisableGLExtensions);
  if (!disabled_gl_extensions_value.empty()) {
    std::vector<base::StringPiece> command_line_disabled_extensions =
        base::SplitStringPiece(disabled_gl_extensions_value, ", ;",
                               base::KEEP_WHITESPACE,
                               base::SPLIT_WANT_NONEMPTY);
    all_disabled_extensions.insert(command_line_disabled_extensions.begin(),
                                   command_line_disabled_extensions.end());
  }

  std::set<int> enabled_driver_bug_workarounds;
  std::vector<std::string> driver_bug_disabled_extensions;
  if (!command_line.HasSwitch(switches::kDisableGpuDriverBugWorkarounds)) {
    std::unique_ptr<gpu::GpuDriverBugList> list(GpuDriverBugList::Create());
    enabled_driver_bug_workarounds =
        list->MakeDecision(GpuControlList::kOsAny, std::string(), gpu_info);
    gpu_feature_info.applied_gpu_driver_bug_list_entries =
        list->GetActiveEntries();

    driver_bug_disabled_extensions = list->GetDisabledExtensions();
    all_disabled_extensions.insert(driver_bug_disabled_extensions.begin(),
                                   driver_bug_disabled_extensions.end());
  }
  gpu::GpuDriverBugList::AppendWorkaroundsFromCommandLine(
      &enabled_driver_bug_workarounds, command_line);

  gpu_feature_info.enabled_gpu_driver_bug_workarounds.insert(
      gpu_feature_info.enabled_gpu_driver_bug_workarounds.begin(),
      enabled_driver_bug_workarounds.begin(),
      enabled_driver_bug_workarounds.end());

  if (all_disabled_extensions.size()) {
    std::vector<base::StringPiece> v(all_disabled_extensions.begin(),
                                     all_disabled_extensions.end());
    gpu_feature_info.disabled_extensions = base::JoinString(v, " ");
  }

  return gpu_feature_info;
}

void SetKeysForCrashLogging(const GPUInfo& gpu_info) {
#if !defined(OS_ANDROID)
  base::debug::SetCrashKeyValue(
      crash_keys::kGPUVendorID,
      base::StringPrintf("0x%04x", gpu_info.gpu.vendor_id));
  base::debug::SetCrashKeyValue(
      crash_keys::kGPUDeviceID,
      base::StringPrintf("0x%04x", gpu_info.gpu.device_id));
#endif
  base::debug::SetCrashKeyValue(crash_keys::kGPUDriverVersion,
                                gpu_info.driver_version);
  base::debug::SetCrashKeyValue(crash_keys::kGPUPixelShaderVersion,
                                gpu_info.pixel_shader_version);
  base::debug::SetCrashKeyValue(crash_keys::kGPUVertexShaderVersion,
                                gpu_info.vertex_shader_version);
#if defined(OS_MACOSX)
  base::debug::SetCrashKeyValue(crash_keys::kGPUGLVersion, gpu_info.gl_version);
#elif defined(OS_POSIX)
  base::debug::SetCrashKeyValue(crash_keys::kGPUVendor, gpu_info.gl_vendor);
  base::debug::SetCrashKeyValue(crash_keys::kGPURenderer, gpu_info.gl_renderer);
#endif
}

}  // namespace gpu
