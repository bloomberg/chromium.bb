// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/gpu/aw_content_gpu_client.h"

namespace android_webview {

AwContentGpuClient::AwContentGpuClient(
    const GetSyncPointManagerCallback& sync_point_manager_callback,
    const GetGPUInfoCallback gpu_info_callback,
    const GetGpuFeatureInfoCallback gpu_feature_info_callback)
    : sync_point_manager_callback_(sync_point_manager_callback),
      gpu_info_callback_(gpu_info_callback),
      gpu_feature_info_callback_(gpu_feature_info_callback) {}

AwContentGpuClient::~AwContentGpuClient() {}

gpu::SyncPointManager* AwContentGpuClient::GetSyncPointManager() {
  return sync_point_manager_callback_.Run();
}

const gpu::GPUInfo* AwContentGpuClient::GetGPUInfo() {
  return &(gpu_info_callback_.Run());
}

const gpu::GpuFeatureInfo* AwContentGpuClient::GetGpuFeatureInfo() {
  return &(gpu_feature_info_callback_.Run());
}

}  // namespace android_webview
