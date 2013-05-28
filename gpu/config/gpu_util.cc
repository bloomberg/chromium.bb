// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_util.h"

#include <vector>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "gpu/command_buffer/service/gpu_switches.h"
#include "gpu/config/gpu_control_list_jsons.h"
#include "gpu/config/gpu_driver_bug_list.h"
#include "gpu/config/gpu_info_collector.h"
#include "ui/gl/gl_switches.h"

namespace gpu {

namespace {

// Combine the integers into a string, seperated by ','.
std::string IntSetToString(const std::set<int>& list) {
  std::string rt;
  for (std::set<int>::const_iterator it = list.begin();
       it != list.end(); ++it) {
    if (!rt.empty())
      rt += ",";
    rt += base::IntToString(*it);
  }
  return rt;
}

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

void MergeFeatureSets(std::set<int>* dst, const std::set<int>& src) {
  DCHECK(dst);
  if (src.empty())
    return;
  dst->insert(src.begin(), src.end());
}

void ApplyGpuDriverBugWorkarounds(CommandLine* command_line) {
  GPUInfo gpu_info;
  CollectBasicGraphicsInfo(&gpu_info);

  GpuDriverBugList* list = GpuDriverBugList::Create();
  list->LoadList("0", kGpuDriverBugListJson,
                 GpuControlList::kCurrentOsOnly);
  std::set<int> workarounds = list->MakeDecision(
      GpuControlList::kOsAny, std::string(), gpu_info);
  if (!workarounds.empty()) {
    command_line->AppendSwitchASCII(switches::kGpuDriverBugWorkarounds,
                                    IntSetToString(workarounds));
  }
}

}  // namespace gpu
