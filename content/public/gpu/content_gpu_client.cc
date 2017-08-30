// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/public/gpu/content_gpu_client.h"

namespace content {

gpu::SyncPointManager* ContentGpuClient::GetSyncPointManager() {
  return nullptr;
}

const gpu::GPUInfo* ContentGpuClient::GetGPUInfo() {
  return nullptr;
}

const gpu::GpuFeatureInfo* ContentGpuClient::GetGpuFeatureInfo() {
  return nullptr;
}

}  // namespace content
