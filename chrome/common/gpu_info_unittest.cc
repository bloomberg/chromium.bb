// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/gpu_info.h"
#include "testing/gtest/include/gtest/gtest.h"

// Test that an empty GPUInfo has valid members
TEST(GPUInfoBasicTest, EmptyGPUInfo) {
  GPUInfo gpu_info;
  EXPECT_EQ(gpu_info.progress(), GPUInfo::kUninitialized);
  EXPECT_EQ(gpu_info.initialization_time().ToInternalValue(), 0);
  EXPECT_EQ(gpu_info.vendor_id(), 0u);
  EXPECT_EQ(gpu_info.device_id(), 0u);
  EXPECT_EQ(gpu_info.driver_vendor(), "");
  EXPECT_EQ(gpu_info.driver_version(), "");
  EXPECT_EQ(gpu_info.pixel_shader_version(), 0u);
  EXPECT_EQ(gpu_info.vertex_shader_version(), 0u);
  EXPECT_EQ(gpu_info.gl_version(), 0u);
  EXPECT_EQ(gpu_info.gl_version_string(), "");
  EXPECT_EQ(gpu_info.gl_vendor(), "");
  EXPECT_EQ(gpu_info.gl_renderer(), "");
  EXPECT_EQ(gpu_info.can_lose_context(), false);
}
