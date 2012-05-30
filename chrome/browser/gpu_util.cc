// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gpu_util.h"

#include <vector>

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/sys_info.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/gpu_blacklist.h"
#include "content/public/browser/gpu_data_manager.h"
#include "content/public/common/content_switches.h"
#include "content/public/common/gpu_info.h"

using content::GpuDataManager;
using content::GpuFeatureType;

namespace {

const char kGpuFeatureNameAccelerated2dCanvas[] = "accelerated_2d_canvas";
const char kGpuFeatureNameAcceleratedCompositing[] = "accelerated_compositing";
const char kGpuFeatureNameWebgl[] = "webgl";
const char kGpuFeatureNameMultisampling[] = "multisampling";
const char kGpuFeatureNameAll[] = "all";
const char kGpuFeatureNameUnknown[] = "unknown";

enum GpuFeatureStatus {
    kGpuFeatureEnabled = 0,
    kGpuFeatureBlacklisted = 1,
    kGpuFeatureDisabled = 2,  // disabled by user but not blacklisted
    kGpuFeatureNumStatus
};

struct GpuFeatureInfo {
  std::string name;
  uint32 blocked;
  bool disabled;
  std::string disabled_description;
  bool fallback_to_software;
};

// Determine if accelerated-2d-canvas is supported, which depends on whether
// lose_context could happen and whether skia is the backend.
bool SupportsAccelerated2dCanvas() {
  if (GpuDataManager::GetInstance()->GetGPUInfo().can_lose_context)
    return false;
#if defined(USE_SKIA)
  return true;
#else
  return false;
#endif
}

DictionaryValue* NewDescriptionValuePair(const std::string& desc,
    const std::string& value) {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetString("description", desc);
  dict->SetString("value", value);
  return dict;
}

DictionaryValue* NewDescriptionValuePair(const std::string& desc,
    Value* value) {
  DictionaryValue* dict = new DictionaryValue();
  dict->SetString("description", desc);
  dict->Set("value", value);
  return dict;
}

Value* NewStatusValue(const char* name, const char* status) {
  DictionaryValue* value = new DictionaryValue();
  value->SetString("name", name);
  value->SetString("status", status);
  return value;
}

std::string GPUDeviceToString(const content::GPUInfo::GPUDevice& gpu) {
  std::string vendor = base::StringPrintf("0x%04x", gpu.vendor_id);
  if (!gpu.vendor_string.empty())
    vendor += " [" + gpu.vendor_string + "]";
  std::string device = base::StringPrintf("0x%04x", gpu.device_id);
  if (!gpu.device_string.empty())
    device += " [" + gpu.device_string + "]";
  return base::StringPrintf(
      "VENDOR = %s, DEVICE= %s", vendor.c_str(), device.c_str());
}

#if defined(OS_WIN)

enum WinSubVersion {
  kWinOthers = 0,
  kWinXP,
  kWinVista,
  kWin7,
  kNumWinSubVersions
};

// Output DxDiagNode tree as nested array of {description,value} pairs
ListValue* DxDiagNodeToList(const content::DxDiagNode& node) {
  ListValue* list = new ListValue();
  for (std::map<std::string, std::string>::const_iterator it =
      node.values.begin();
      it != node.values.end();
      ++it) {
    list->Append(NewDescriptionValuePair(it->first, it->second));
  }

  for (std::map<std::string, content::DxDiagNode>::const_iterator it =
      node.children.begin();
      it != node.children.end();
      ++it) {
    ListValue* sublist = DxDiagNodeToList(it->second);
    list->Append(NewDescriptionValuePair(it->first, sublist));
  }
  return list;
}

int GetGpuBlacklistHistogramValueWin(GpuFeatureStatus status) {
  static WinSubVersion sub_version = kNumWinSubVersions;
  if (sub_version == kNumWinSubVersions) {
    sub_version = kWinOthers;
    std::string version_str = base::SysInfo::OperatingSystemVersion();
    size_t pos = version_str.find_first_not_of("0123456789.");
    if (pos != std::string::npos)
      version_str = version_str.substr(0, pos);
    scoped_ptr<Version> os_version(Version::GetVersionFromString(version_str));
    if (os_version.get() && os_version->components().size() >= 2) {
      const std::vector<uint16>& version_numbers = os_version->components();
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
  else if (feature_string == kGpuFeatureNameAcceleratedCompositing)
    return content::GPU_FEATURE_TYPE_ACCELERATED_COMPOSITING;
  else if (feature_string == kGpuFeatureNameWebgl)
    return content::GPU_FEATURE_TYPE_WEBGL;
  else if (feature_string == kGpuFeatureNameMultisampling)
    return content::GPU_FEATURE_TYPE_MULTISAMPLING;
  else if (feature_string == kGpuFeatureNameAll)
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
    if (!matches.size())
      matches.push_back(kGpuFeatureNameUnknown);
  }
  return JoinString(matches, ',');
}

Value* GetFeatureStatus() {
  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  bool gpu_access_blocked = !GpuDataManager::GetInstance()->GpuAccessAllowed();

  uint32 flags = GpuDataManager::GetInstance()->GetGpuFeatureType();
  DictionaryValue* status = new DictionaryValue();

  const GpuFeatureInfo kGpuFeatureInfo[] = {
      {
          "2d_canvas",
          flags & content::GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS,
          command_line.HasSwitch(switches::kDisableAccelerated2dCanvas) ||
          !SupportsAccelerated2dCanvas(),
          "Accelerated 2D canvas is unavailable: either disabled at the command"
          " line or not supported by the current system.",
          true
      },
      {
          "compositing",
          flags & content::GPU_FEATURE_TYPE_ACCELERATED_COMPOSITING,
          command_line.HasSwitch(switches::kDisableAcceleratedCompositing),
          "Accelerated compositing has been disabled, either via about:flags or"
          " command line. This adversely affects performance of all hardware"
          " accelerated features.",
          true
      },
      {
          "3d_css",
          flags & content::GPU_FEATURE_TYPE_ACCELERATED_COMPOSITING,
          command_line.HasSwitch(switches::kDisableAcceleratedLayers),
          "Accelerated layers have been disabled at the command line.",
          false
      },
      {
          "css_animation",
          flags & content::GPU_FEATURE_TYPE_ACCELERATED_COMPOSITING,
          command_line.HasSwitch(switches::kDisableThreadedAnimation) ||
          command_line.HasSwitch(switches::kDisableAcceleratedCompositing),
          "Accelerated CSS animation has been disabled at the command line.",
          true
      },
      {
          "webgl",
          flags & content::GPU_FEATURE_TYPE_WEBGL,
          command_line.HasSwitch(switches::kDisableExperimentalWebGL),
          "WebGL has been disabled, either via about:flags or command line.",
          false
      },
      {
          "multisampling",
          flags & content::GPU_FEATURE_TYPE_MULTISAMPLING,
          command_line.HasSwitch(switches::kDisableGLMultisampling),
          "Multisampling has been disabled, either via about:flags or command"
          " line.",
          false
      }
  };
  const size_t kNumFeatures = sizeof(kGpuFeatureInfo) / sizeof(GpuFeatureInfo);

  // Build the feature_status field.
  {
    ListValue* feature_status_list = new ListValue();

    for (size_t i = 0; i < kNumFeatures; ++i) {
      std::string status;
      if (kGpuFeatureInfo[i].disabled) {
        status = "disabled";
        if (kGpuFeatureInfo[i].name == "css_animation") {
          status += "_software_animated";
        } else {
          if (kGpuFeatureInfo[i].fallback_to_software)
            status += "_software";
          else
            status += "_off";
        }
      } else if (GpuDataManager::GetInstance()->ShouldUseSoftwareRendering()) {
        status = "unavailable_software";
      } else if (kGpuFeatureInfo[i].blocked ||
                 gpu_access_blocked) {
        status = "unavailable";
        if (kGpuFeatureInfo[i].fallback_to_software)
          status += "_software";
        else
          status += "_off";
      } else {
        status = "enabled";
        if (kGpuFeatureInfo[i].name == "webgl" &&
            (command_line.HasSwitch(switches::kDisableAcceleratedCompositing) ||
             (flags & content::GPU_FEATURE_TYPE_ACCELERATED_COMPOSITING)))
          status += "_readback";
        bool has_thread = CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kEnableThreadedCompositing) &&
            (!CommandLine::ForCurrentProcess()->HasSwitch(
                switches::kDisableThreadedCompositing));
        if (kGpuFeatureInfo[i].name == "compositing" &&
            CommandLine::ForCurrentProcess()->HasSwitch(
                switches::kForceCompositingMode))
          status += "_force";
        if (kGpuFeatureInfo[i].name == "compositing" &&
            has_thread)
          status += "_threaded";
        if (kGpuFeatureInfo[i].name == "css_animation") {
          if (has_thread)
            status = "accelerated_threaded";
          else
            status = "accelerated";
        }
      }
      feature_status_list->Append(
          NewStatusValue(kGpuFeatureInfo[i].name.c_str(), status.c_str()));
    }

    status->Set("featureStatus", feature_status_list);
  }

  // Build the problems list.
  {
    ListValue* problem_list = new ListValue();

    if (gpu_access_blocked) {
      DictionaryValue* problem = new DictionaryValue();
      problem->SetString("description",
          "GPU process was unable to boot. Access to GPU disallowed.");
      problem->Set("crBugs", new ListValue());
      problem->Set("webkitBugs", new ListValue());
      problem_list->Append(problem);
    }

    for (size_t i = 0; i < kNumFeatures; ++i) {
      if (kGpuFeatureInfo[i].disabled) {
        DictionaryValue* problem = new DictionaryValue();
        problem->SetString(
            "description", kGpuFeatureInfo[i].disabled_description);
        problem->Set("crBugs", new ListValue());
        problem->Set("webkitBugs", new ListValue());
        problem_list->Append(problem);
      }
    }

    GpuBlacklist::GetInstance()->GetBlacklistReasons(problem_list);

    status->Set("problems", problem_list);
  }

  return status;
}

DictionaryValue* GpuInfoAsDictionaryValue() {
  content::GPUInfo gpu_info = GpuDataManager::GetInstance()->GetGPUInfo();
  ListValue* basic_info = new ListValue();
  basic_info->Append(NewDescriptionValuePair(
      "Initialization time",
      base::Int64ToString(gpu_info.initialization_time.InMilliseconds())));
  basic_info->Append(NewDescriptionValuePair(
      "GPU0", GPUDeviceToString(gpu_info.gpu)));
  for (size_t i = 0; i < gpu_info.secondary_gpus.size(); ++i) {
    basic_info->Append(NewDescriptionValuePair(
        base::StringPrintf("GPU%d", static_cast<int>(i + 1)),
        GPUDeviceToString(gpu_info.secondary_gpus[i])));
  }
  basic_info->Append(NewDescriptionValuePair(
      "Optimus", Value::CreateBooleanValue(gpu_info.optimus)));
  basic_info->Append(NewDescriptionValuePair(
      "AMD switchable", Value::CreateBooleanValue(gpu_info.amd_switchable)));
  basic_info->Append(NewDescriptionValuePair("Driver vendor",
                                             gpu_info.driver_vendor));
  basic_info->Append(NewDescriptionValuePair("Driver version",
                                             gpu_info.driver_version));
  basic_info->Append(NewDescriptionValuePair("Driver date",
                                             gpu_info.driver_date));
  basic_info->Append(NewDescriptionValuePair("Pixel shader version",
                                             gpu_info.pixel_shader_version));
  basic_info->Append(NewDescriptionValuePair("Vertex shader version",
                                             gpu_info.vertex_shader_version));
  basic_info->Append(NewDescriptionValuePair("GL version",
                                             gpu_info.gl_version));
  basic_info->Append(NewDescriptionValuePair("GL_VENDOR",
                                             gpu_info.gl_vendor));
  basic_info->Append(NewDescriptionValuePair("GL_RENDERER",
                                             gpu_info.gl_renderer));
  basic_info->Append(NewDescriptionValuePair("GL_VERSION",
                                             gpu_info.gl_version_string));
  basic_info->Append(NewDescriptionValuePair("GL_EXTENSIONS",
                                             gpu_info.gl_extensions));

  DictionaryValue* info = new DictionaryValue();
  info->Set("basic_info", basic_info);

#if defined(OS_WIN)
  ListValue* perf_info = new ListValue();
  perf_info->Append(NewDescriptionValuePair(
      "Graphics",
      base::StringPrintf("%.1f", gpu_info.performance_stats.graphics)));
  perf_info->Append(NewDescriptionValuePair(
      "Gaming",
      base::StringPrintf("%.1f", gpu_info.performance_stats.gaming)));
  perf_info->Append(NewDescriptionValuePair(
      "Overall",
      base::StringPrintf("%.1f", gpu_info.performance_stats.overall)));
  info->Set("performance_info", perf_info);

  Value* dx_info;
  if (gpu_info.dx_diagnostics.children.size())
    dx_info = DxDiagNodeToList(gpu_info.dx_diagnostics);
  else
    dx_info = Value::CreateNullValue();
  info->Set("diagnostics", dx_info);
#endif

  return info;
}

void UpdateStats() {
  GpuBlacklist* blacklist = GpuBlacklist::GetInstance();
  uint32 max_entry_id = blacklist->max_entry_id();
  if (max_entry_id == 0) {
    // GPU Blacklist was not loaded.  No need to go further.
    return;
  }

  const CommandLine& command_line = *CommandLine::ForCurrentProcess();
  uint32 flags = GpuDataManager::GetInstance()->GetGpuFeatureType();
  bool disabled = false;
  if (flags == 0) {
    UMA_HISTOGRAM_ENUMERATION("GPU.BlacklistTestResultsPerEntry",
        0, max_entry_id + 1);
  } else {
    std::vector<uint32> flag_entries;
    blacklist->GetGpuFeatureTypeEntries(
        content::GPU_FEATURE_TYPE_ALL, flag_entries, disabled);
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
  blacklist->GetGpuFeatureTypeEntries(
      content::GPU_FEATURE_TYPE_ALL, flag_disabled_entries, disabled);
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
      command_line.HasSwitch(switches::kDisableExperimentalWebGL)
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
    if (flags & kGpuFeatures[i])
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
