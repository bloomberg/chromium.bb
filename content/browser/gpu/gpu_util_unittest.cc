// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_switches.h"

namespace content {

TEST(GpuUtilTest, GpuSwitchingOptionFromString) {
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

TEST(GpuUtilTest, GpuSwitchingOptionToString) {
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

TEST(GpuUtilTest, MergeFeatureSets) {
  {
    // Merge two empty sets.
    std::set<int> src;
    std::set<int> dst;
    EXPECT_TRUE(dst.empty());
    MergeFeatureSets(&dst, src);
    EXPECT_TRUE(dst.empty());
  }
  {
    // Merge an empty set into a set with elements.
    std::set<int> src;
    std::set<int> dst;
    dst.insert(1);
    EXPECT_EQ(1u, dst.size());
    MergeFeatureSets(&dst, src);
    EXPECT_EQ(1u, dst.size());
  }
  {
    // Merge two sets where the source elements are already in the target set.
    std::set<int> src;
    std::set<int> dst;
    src.insert(1);
    dst.insert(1);
    EXPECT_EQ(1u, dst.size());
    MergeFeatureSets(&dst, src);
    EXPECT_EQ(1u, dst.size());
  }
  {
    // Merge two sets with different elements.
    std::set<int> src;
    std::set<int> dst;
    src.insert(1);
    dst.insert(2);
    EXPECT_EQ(1u, dst.size());
    MergeFeatureSets(&dst, src);
    EXPECT_EQ(2u, dst.size());
  }
}

}  // namespace content
