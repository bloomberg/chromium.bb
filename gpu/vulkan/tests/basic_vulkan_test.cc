// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "gpu/vulkan/tests/basic_vulkan_test.h"

#include "gpu/vulkan/tests/native_window.h"
#include "ui/gfx/geometry/rect.h"

namespace gpu {

void BasicVulkanTest::SetUp() {
  const gfx::Rect kDefaultBounds(10, 10, 100, 100);
  window_ = CreateNativeWindow(kDefaultBounds);
  device_queue_.Initialize(VulkanDeviceQueue::GRAPHICS_QUEUE_FLAG |
                           VulkanDeviceQueue::PRESENTATION_SUPPORT_QUEUE_FLAG);
}

void BasicVulkanTest::TearDown() {
  DestroyNativeWindow(window_);
  window_ = gfx::kNullAcceleratedWidget;
  device_queue_.Destroy();
}

}  // namespace gpu
