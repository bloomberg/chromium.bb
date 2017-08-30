// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_GPU_AW_CONTENT_GPU_CLIENT_H_
#define ANDROID_WEBVIEW_GPU_AW_CONTENT_GPU_CLIENT_H_

#include "base/callback.h"
#include "base/macros.h"
#include "content/public/gpu/content_gpu_client.h"

namespace android_webview {

class AwContentGpuClient : public content::ContentGpuClient {
 public:
  using GetSyncPointManagerCallback = base::Callback<gpu::SyncPointManager*()>;
  using GetGPUInfoCallback = base::Callback<const gpu::GPUInfo&()>;
  using GetGpuFeatureInfoCallback =
      base::Callback<const gpu::GpuFeatureInfo&()>;

  explicit AwContentGpuClient(
      const GetSyncPointManagerCallback& sync_point_manager_callback,
      const GetGPUInfoCallback gpu_info_callback,
      const GetGpuFeatureInfoCallback gpu_feature_info_callback);
  ~AwContentGpuClient() override;

  // content::ContentGpuClient implementation.
  gpu::SyncPointManager* GetSyncPointManager() override;
  const gpu::GPUInfo* GetGPUInfo() override;
  const gpu::GpuFeatureInfo* GetGpuFeatureInfo() override;

 private:
  GetSyncPointManagerCallback sync_point_manager_callback_;
  GetGPUInfoCallback gpu_info_callback_;
  GetGpuFeatureInfoCallback gpu_feature_info_callback_;
  DISALLOW_COPY_AND_ASSIGN(AwContentGpuClient);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_GPU_AW_CONTENT_GPU_CLIENT_H_
