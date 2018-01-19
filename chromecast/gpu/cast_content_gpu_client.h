// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_GPU_CAST_CONTENT_GPU_CLIENT_H_
#define CHROMECAST_GPU_CAST_CONTENT_GPU_CLIENT_H_

#include <memory>

#include "base/macros.h"
#include "content/public/gpu/content_gpu_client.h"

namespace gpu {
struct GPUInfo;
}

namespace chromecast {

namespace shell {

class CastContentGpuClient : public content::ContentGpuClient {
 public:
  ~CastContentGpuClient() override;

  static std::unique_ptr<CastContentGpuClient> Create();

  // ContentGpuClient:
  const gpu::GPUInfo* GetGPUInfo() override;

 protected:
  CastContentGpuClient();

 private:
  std::unique_ptr<gpu::GPUInfo> gpu_info_;

  DISALLOW_COPY_AND_ASSIGN(CastContentGpuClient);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_GPU_CAST_CONTENT_GPU_CLIENT_H_
