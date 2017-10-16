// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_info_collector.h"

#include "base/trace_event/trace_event.h"
#include "third_party/angle/src/gpu_info_util/SystemInfo.h"

namespace gpu {

CollectInfoResult CollectContextGraphicsInfo(GPUInfo* gpu_info) {
  DCHECK(gpu_info);

  TRACE_EVENT0("gpu", "gpu_info_collector::CollectGraphicsInfo");

  CollectInfoResult result = CollectGraphicsInfoGL(gpu_info);
  gpu_info->context_info_state = result;
  return result;
}

CollectInfoResult CollectBasicGraphicsInfo(GPUInfo* gpu_info) {
  DCHECK(gpu_info);

  angle::SystemInfo system_info;
  bool success = angle::GetSystemInfo(&system_info);
  FillGPUInfoFromSystemInfo(gpu_info, &system_info);

  if (success) {
    gpu_info->basic_info_state = kCollectInfoSuccess;
  } else {
    gpu_info->basic_info_state = kCollectInfoNonFatalFailure;
  }

  return gpu_info->basic_info_state;
}

CollectInfoResult CollectDriverInfoGL(GPUInfo* gpu_info) {
  DCHECK(gpu_info);

  // Extract the OpenGL driver version string from the GL_VERSION string.
  // Mac OpenGL drivers have the driver version
  // at the end of the gl version string preceded by a dash.
  // Use some jiggery-pokery to turn that utf8 string into a std::wstring.
  size_t pos = gpu_info->gl_version.find_last_of('-');
  if (pos == std::string::npos)
    return kCollectInfoNonFatalFailure;
  gpu_info->driver_version = gpu_info->gl_version.substr(pos + 1);
  return kCollectInfoSuccess;
}

void MergeGPUInfo(GPUInfo* basic_gpu_info,
                  const GPUInfo& context_gpu_info) {
  MergeGPUInfoGL(basic_gpu_info, context_gpu_info);
}

}  // namespace gpu
