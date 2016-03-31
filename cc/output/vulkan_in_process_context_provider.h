// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_OUTPUT_VULKAN_IN_PROCESS_CONTEXT_PROVIDER_H_
#define CC_OUTPUT_VULKAN_IN_PROCESS_CONTEXT_PROVIDER_H_

#include "base/memory/scoped_ptr.h"
#include "cc/base/cc_export.h"
#include "cc/output/vulkan_context_provider.h"

namespace gpu {
class VulkanDeviceQueue;
}

namespace cc {

class CC_EXPORT VulkanInProcessContextProvider : public VulkanContextProvider {
 public:
  static scoped_refptr<VulkanInProcessContextProvider> Create();

  bool Initialize();
  void Destroy();

  gpu::VulkanDeviceQueue* GetDeviceQueue() override;

 protected:
  VulkanInProcessContextProvider();
  ~VulkanInProcessContextProvider() override;

 private:
  scoped_ptr<gpu::VulkanDeviceQueue> device_queue_;
};

}  // namespace cc

#endif  // CC_OUTPUT_VULKAN_IN_PROCESS_CONTEXT_PROVIDER_H_
