// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_switches.h"

namespace content {

TEST(GpuUtilsTest, GpuSwitchingOptionFromString) {
  // Test StringToGpuSwitchingOption.
  EXPECT_EQ(StringToGpuSwitchingOption(
                switches::kGpuSwitchingOptionNameAutomatic),
            GPU_SWITCHING_OPTION_AUTOMATIC);
  EXPECT_EQ(StringToGpuSwitchingOption(
                switches::kGpuSwitchingOptionNameForceDiscrete),
            GPU_SWITCHING_OPTION_FORCE_DISCRETE);
  EXPECT_EQ(StringToGpuSwitchingOption(
                switches::kGpuSwitchingOptionNameForceIntegrated),
            GPU_SWITCHING_OPTION_FORCE_INTEGRATED);
  EXPECT_EQ(StringToGpuSwitchingOption("xxx"), GPU_SWITCHING_OPTION_UNKNOWN);
}

TEST(GpuUtilsTest, GpuSwitchingOptionToString) {
  // Test GpuSwitchingOptionToString.
  EXPECT_STREQ(
      GpuSwitchingOptionToString(GPU_SWITCHING_OPTION_AUTOMATIC).c_str(),
      switches::kGpuSwitchingOptionNameAutomatic);
  EXPECT_STREQ(
      GpuSwitchingOptionToString(GPU_SWITCHING_OPTION_FORCE_DISCRETE).c_str(),
      switches::kGpuSwitchingOptionNameForceDiscrete);
  EXPECT_STREQ(
      GpuSwitchingOptionToString(GPU_SWITCHING_OPTION_FORCE_INTEGRATED).c_str(),
      switches::kGpuSwitchingOptionNameForceIntegrated);
}

}  // namespace content
