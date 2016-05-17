// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef GPU_VULKAN_TESTS_BASIC_VULKAN_TEST_H_
#define GPU_VULKAN_TESTS_BASIC_VULKAN_TEST_H_

#include "gpu/vulkan/vulkan_device_queue.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/native_widget_types.h"

namespace gpu {

class BasicVulkanTest : public testing::Test {
 public:
  void SetUp() override;
  void TearDown() override;

  gfx::AcceleratedWidget window() const { return window_; }
  VulkanDeviceQueue* GetDeviceQueue() { return &device_queue_; }

 private:
  VulkanDeviceQueue device_queue_;
  gfx::AcceleratedWidget window_ = gfx::kNullAcceleratedWidget;
};

}  // namespace gpu

#endif  // GPU_VULKAN_TESTS_BASIC_VULKAN_TEST_H_
