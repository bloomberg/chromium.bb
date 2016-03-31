// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/output/vulkan_in_process_context_provider.h"

#include <vector>

#include "gpu/vulkan/vulkan_device_queue.h"

#if defined(VK_USE_PLATFORM_XLIB_KHR)
#include "ui/gfx/x/x11_types.h"
#endif  // defined(VK_USE_PLATFORM_XLIB_KHR)

namespace cc {

scoped_refptr<VulkanInProcessContextProvider>
VulkanInProcessContextProvider::Create() {
  scoped_refptr<VulkanInProcessContextProvider> context_provider(
      new VulkanInProcessContextProvider);
  if (!context_provider->Initialize())
    return nullptr;
  return context_provider;
}

bool VulkanInProcessContextProvider::Initialize() {
  scoped_ptr<gpu::VulkanDeviceQueue> device_queue(new gpu::VulkanDeviceQueue);
  if (device_queue->Initialize(
          gpu::VulkanDeviceQueue::GRAPHICS_QUEUE_FLAG |
          gpu::VulkanDeviceQueue::PRESENTATION_SUPPORT_QUEUE_FLAG)) {
    device_queue_ = std::move(device_queue);
    return true;
  }

  return false;
}

void VulkanInProcessContextProvider::Destroy() {
  if (device_queue_) {
    device_queue_->Destroy();
    device_queue_.reset();
  }
}

gpu::VulkanDeviceQueue* VulkanInProcessContextProvider::GetDeviceQueue() {
  return device_queue_.get();
}

VulkanInProcessContextProvider::VulkanInProcessContextProvider() {}

VulkanInProcessContextProvider::~VulkanInProcessContextProvider() {}

}  // namespace cc
