// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_info_collector.h"

#include "base/logging.h"

namespace gpu {

bool CollectContextGraphicsInfo(GPUInfo* gpu_info) {
  return CollectBasicGraphicsInfo(gpu_info);
}

GpuIDResult CollectGpuID(uint32* vendor_id, uint32* device_id) {
  DCHECK(vendor_id && device_id);
  *vendor_id = 0;
  *device_id = 0;
  return kGpuIDNotSupported;
}

bool CollectBasicGraphicsInfo(GPUInfo* gpu_info) {
  gpu_info->can_lose_context = false;
  return true;
}

bool CollectDriverInfoGL(GPUInfo* gpu_info) {
  NOTIMPLEMENTED();
  return false;
}

void MergeGPUInfo(GPUInfo* basic_gpu_info,
                  const GPUInfo& context_gpu_info) {
  MergeGPUInfoGL(basic_gpu_info, context_gpu_info);
}

bool DetermineActiveGPU(GPUInfo* gpu_info) {
  DCHECK(gpu_info);
  if (gpu_info->secondary_gpus.size() == 0)
    return true;
  // TODO(zmo): implement this.
  return false;
}

}  // namespace gpu_info_collector
