// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/gpu/cast_content_gpu_client.h"

#include <memory>

#include "build/build_config.h"
#include "chromecast/base/cast_sys_info_util.h"
#include "chromecast/public/cast_sys_info.h"
#include "gpu/config/gpu_info.h"
#include "gpu/config/gpu_info_collector.h"

namespace chromecast {

namespace shell {

// static
std::unique_ptr<CastContentGpuClient> CastContentGpuClient::Create() {
  return base::WrapUnique(new CastContentGpuClient());
}

CastContentGpuClient::CastContentGpuClient() = default;

CastContentGpuClient::~CastContentGpuClient() = default;

const gpu::GPUInfo* CastContentGpuClient::GetGPUInfo() {
#if defined(OS_ANDROID)
  return nullptr;
#else
  if (!gpu_info_) {
    gpu_info_ = std::make_unique<gpu::GPUInfo>();
    std::unique_ptr<CastSysInfo> sys_info = CreateSysInfo();
    gpu_info_->gl_vendor = sys_info->GetGlVendor();
    gpu_info_->gl_renderer = sys_info->GetGlRenderer();
    gpu_info_->gl_version = sys_info->GetGlVersion();
    gpu::CollectDriverInfoGL(gpu_info_.get());
  }
  return gpu_info_.get();
#endif  // OS_ANDROID
}

}  // namespace shell
}  // namespace chromecast
