// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/config/gpu_info_collector.h"

namespace gpu {

CollectInfoResult CollectContextGraphicsInfo(GPUInfo* gpu_info) {
  // TODO(crbug.com/707031): Implement this.
  NOTIMPLEMENTED();
  return kCollectInfoFatalFailure;
}

CollectInfoResult CollectBasicGraphicsInfo(GPUInfo* gpu_info) {
  // TODO(crbug.com/707031): Implement this.
  NOTIMPLEMENTED();
  return kCollectInfoFatalFailure;
}

CollectInfoResult CollectDriverInfoGL(GPUInfo* gpu_info) {
  // TODO(crbug.com/707031): Implement this.
  NOTIMPLEMENTED();
  return kCollectInfoFatalFailure;
}

void MergeGPUInfo(GPUInfo* basic_gpu_info, const GPUInfo& context_gpu_info) {
  MergeGPUInfoGL(basic_gpu_info, context_gpu_info);
}

}  // namespace gpu
