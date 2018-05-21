// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/command_line.h"
#include "base/strings/string_split.h"
#include "base/trace_event/trace_event.h"
#include "gpu/config/gpu_info_collector.h"
#include "gpu/config/gpu_switches.h"
#include "third_party/angle/src/gpu_info_util/SystemInfo.h"
#include "third_party/re2/src/re2/re2.h"

namespace gpu {

bool CollectContextGraphicsInfo(GPUInfo* gpu_info) {
  DCHECK(gpu_info);

  TRACE_EVENT0("gpu", "gpu_info_collector::CollectGraphicsInfo");

  return CollectGraphicsInfoGL(gpu_info);
}

bool CollectBasicGraphicsInfo(GPUInfo* gpu_info) {
  DCHECK(gpu_info);

  angle::SystemInfo system_info;
  bool success = angle::GetSystemInfo(&system_info);
  FillGPUInfoFromSystemInfo(gpu_info, &system_info);
  return success;
}

void CollectDriverInfoGL(GPUInfo* gpu_info) {
  DCHECK(gpu_info);

  if (!gpu_info->driver_vendor.empty() && !gpu_info->driver_version.empty())
    return;

  std::string gl_version = gpu_info->gl_version;
  std::vector<std::string> pieces = base::SplitString(
      gl_version, base::kWhitespaceASCII, base::KEEP_WHITESPACE,
      base::SPLIT_WANT_NONEMPTY);
  // In linux, the gl version string might be in the format of
  //   GLVersion DriverVendor DriverVersion
  if (pieces.size() < 3)
    return;

  // Search from the end for the first piece that starts with major.minor or
  // major.minor.micro but assume the driver version cannot be in the first two
  // pieces.
  re2::RE2 pattern("([\\d]+\\.[\\d]+(\\.[\\d]+)?).*");
  std::string driver_version;
  auto it = pieces.rbegin();
  while (pieces.rend() - it > 2) {
    bool parsed = re2::RE2::FullMatch(*it, pattern, &driver_version);
    if (parsed)
      break;
    ++it;
  }

  if (driver_version.empty())
    return;

  gpu_info->driver_vendor = *(++it);
  gpu_info->driver_version = driver_version;
}

}  // namespace gpu
