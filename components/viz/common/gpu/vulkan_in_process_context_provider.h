// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_COMMON_GPU_VULKAN_IN_PROCESS_CONTEXT_PROVIDER_H_
#define COMPONENTS_VIZ_COMMON_GPU_VULKAN_IN_PROCESS_CONTEXT_PROVIDER_H_

#include <memory>

#include "components/viz/common/gpu/vulkan_context_provider.h"
#include "components/viz/common/viz_common_export.h"
#include "gpu/vulkan/features.h"

namespace gpu {
class VulkanDeviceQueue;
}

namespace viz {

class VIZ_COMMON_EXPORT VulkanInProcessContextProvider
    : public VulkanContextProvider {
 public:
  static scoped_refptr<VulkanInProcessContextProvider> Create();

  bool Initialize();
  void Destroy();

  // VulkanContextProvider implementation
  gpu::VulkanDeviceQueue* GetDeviceQueue() override;

 protected:
  VulkanInProcessContextProvider();
  ~VulkanInProcessContextProvider() override;

 private:
#if BUILDFLAG(ENABLE_VULKAN)
  std::unique_ptr<gpu::VulkanDeviceQueue> device_queue_;
#endif

  DISALLOW_COPY_AND_ASSIGN(VulkanInProcessContextProvider);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_COMMON_GPU_VULKAN_IN_PROCESS_CONTEXT_PROVIDER_H_
