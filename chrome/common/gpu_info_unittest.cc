// Copyright (c) 2006-2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "chrome/common/gpu_info.h"
#include "testing/gtest/include/gtest/gtest.h"

// Test that an empty GPUInfo has valid members
TEST(GPUInfoBasicTest, EmptyGPUInfo) {
  GPUInfo gpu_info;
  EXPECT_EQ(gpu_info.vendor_id(), 0u);
  EXPECT_EQ(gpu_info.device_id(), 0u);
  EXPECT_EQ(gpu_info.driver_version(), L"");
  EXPECT_EQ(gpu_info.pixel_shader_version(), 0u);
  EXPECT_EQ(gpu_info.vertex_shader_version(), 0u);
}
