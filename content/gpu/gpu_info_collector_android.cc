// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/gpu_info_collector.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/string_piece.h"
#include "base/string_split.h"
#include "base/string_util.h"
#include "content/public/common/content_switches.h"

namespace {

std::string GetDriverVersionFromString(const std::string& version_string) {
  // Extract driver version from the second number in a string like:
  // "OpenGL ES 2.0 V@6.0 AU@ (CL@2946718)"

  // Exclude first "2.0".
  size_t begin = version_string.find_first_of("0123456789");
  if (begin == std::string::npos)
    return "0";
  size_t end = version_string.find_first_not_of("01234567890.", begin);

  // Extract number of the form "%d.%d"
  begin = version_string.find_first_of("0123456789", end);
  if (begin == std::string::npos)
    return "0";
  end = version_string.find_first_not_of("01234567890.", begin);
  std::string sub_string;
  if (end != std::string::npos)
    sub_string = version_string.substr(begin, end - begin);
  else
    sub_string = version_string.substr(begin);
  std::vector<std::string> pieces;
  base::SplitString(sub_string, '.', &pieces);
  if (pieces.size() < 2)
    return "0";
  return pieces[0] + "." + pieces[1];
}

}

namespace gpu_info_collector {

bool CollectContextGraphicsInfo(content::GPUInfo* gpu_info) {
  // can_lose_context must be false to enable accelerated Canvas2D
  gpu_info->can_lose_context = false;
  gpu_info->finalized = true;
  return CollectGraphicsInfoGL(gpu_info);
}

bool CollectBasicGraphicsInfo(content::GPUInfo* gpu_info) {
  gpu_info->can_lose_context = false;
  // Create a short-lived context on the UI thread to collect the GL strings.
  if (!CollectGraphicsInfoGL(gpu_info))
    return false;

  std::string vendor(StringToLowerASCII(gpu_info->gl_vendor));
  std::string renderer(StringToLowerASCII(gpu_info->gl_renderer));
  bool is_img = vendor.find("imagination") != std::string::npos;
  bool is_arm = vendor.find("arm") != std::string::npos;
  bool is_qualcomm = vendor.find("qualcomm") != std::string::npos;
  bool is_mali_t604 = is_arm && renderer.find("mali-t604") != std::string::npos;

  // IMG: avoid context switching perf problems, crashes with share groups
  // Mali-T604: http://crbug.com/154715
  // QualComm: Crashes with share groups
  if (is_img || is_mali_t604 || is_qualcomm) {
    CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kEnableVirtualGLContexts);
  }
  return true;
}

bool CollectDriverInfoGL(content::GPUInfo* gpu_info) {
  gpu_info->driver_version = GetDriverVersionFromString(
      gpu_info->gl_version_string);
  return true;
}

void MergeGPUInfo(content::GPUInfo* basic_gpu_info,
                  const content::GPUInfo& context_gpu_info) {
  MergeGPUInfoGL(basic_gpu_info, context_gpu_info);
}

}  // namespace gpu_info_collector
