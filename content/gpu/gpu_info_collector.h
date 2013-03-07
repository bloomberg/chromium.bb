// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_GPU_GPU_INFO_COLLECTOR_H_
#define CONTENT_GPU_GPU_INFO_COLLECTOR_H_

#include "base/basictypes.h"
#include "build/build_config.h"
#include "content/common/content_export.h"
#include "content/public/common/gpu_info.h"

namespace gpu_info_collector {

// Collect GPU vendor_id and device ID.
CONTENT_EXPORT bool CollectGpuID(uint32* vendor_id, uint32* device_id);

// Collects basic GPU info without creating a GL/DirectX context (and without
// the danger of crashing), including vendor_id and device_id.
// This is called at browser process startup time.
// The subset each platform collects may be different.
CONTENT_EXPORT bool CollectBasicGraphicsInfo(
    content::GPUInfo* gpu_info);

// Create a GL/DirectX context and collect related info.
// This is called at GPU process startup time.
// Returns true on success.
bool CollectContextGraphicsInfo(content::GPUInfo* gpu_info);

#if defined(OS_WIN)
// Collect the DirectX Disagnostics information about the attached displays.
bool GetDxDiagnostics(content::DxDiagNode* output);
#endif  // OS_WIN

// Create a GL context and collect GL strings and versions.
CONTENT_EXPORT bool CollectGraphicsInfoGL(content::GPUInfo* gpu_info);

// Each platform stores the driver version on the GL_VERSION string differently
bool CollectDriverInfoGL(content::GPUInfo* gpu_info);

// Merge GPUInfo from CollectContextGraphicsInfo into basic GPUInfo.
// This is platform specific, depending on which info are collected at which
// stage.
void MergeGPUInfo(content::GPUInfo* basic_gpu_info,
                  const content::GPUInfo& context_gpu_info);

// MergeGPUInfo() when GL driver is used.
void MergeGPUInfoGL(content::GPUInfo* basic_gpu_info,
                    const content::GPUInfo& context_gpu_info);

// Advanced Micro Devices has interesting configurations on laptops were
// there are two videocards that can alternatively a given process output.
enum AMDVideoCardType {
  UNKNOWN,
  STANDALONE,
  INTEGRATED,
  SWITCHABLE
};

}  // namespace gpu_info_collector

#endif  // CONTENT_GPU_GPU_INFO_COLLECTOR_H_
