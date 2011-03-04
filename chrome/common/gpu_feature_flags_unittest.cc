// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/gpu_feature_flags.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(GpuFeatureFlagsTest, GpuFeatureFlagsBasic) {
  // Test that by default all flags are set to false.
  GpuFeatureFlags flags;
  EXPECT_EQ(flags.flags(), 0u);

  // Test SetFlags().
  GpuFeatureFlags flags2;
  flags2.set_flags(GpuFeatureFlags::kGpuFeatureAcceleratedCompositing |
                   GpuFeatureFlags::kGpuFeatureWebgl);
  EXPECT_EQ(flags2.flags(),
            static_cast<uint32>(
                GpuFeatureFlags::kGpuFeatureAcceleratedCompositing |
                GpuFeatureFlags::kGpuFeatureWebgl));

  // Test Combine() is basically OR operation per flag.
  flags.set_flags(GpuFeatureFlags::kGpuFeatureAccelerated2dCanvas);
  flags.Combine(flags2);
  EXPECT_EQ(flags.flags(),
            static_cast<uint32>(
                GpuFeatureFlags::kGpuFeatureAccelerated2dCanvas |
                GpuFeatureFlags::kGpuFeatureAcceleratedCompositing |
                GpuFeatureFlags::kGpuFeatureWebgl));

  // Test the currently supported feature set.
  flags.set_flags(GpuFeatureFlags::kGpuFeatureAll);
  EXPECT_EQ(flags.flags(),
            static_cast<uint32>(
                GpuFeatureFlags::kGpuFeatureAccelerated2dCanvas |
                GpuFeatureFlags::kGpuFeatureAcceleratedCompositing |
                GpuFeatureFlags::kGpuFeatureWebgl |
                GpuFeatureFlags::kGpuFeatureMultisampling));

  // Test StringToGpuFeatureType.
  EXPECT_EQ(GpuFeatureFlags::StringToGpuFeatureType("accelerated_2d_canvas"),
            GpuFeatureFlags::kGpuFeatureAccelerated2dCanvas);
  EXPECT_EQ(GpuFeatureFlags::StringToGpuFeatureType("accelerated_compositing"),
            GpuFeatureFlags::kGpuFeatureAcceleratedCompositing);
  EXPECT_EQ(GpuFeatureFlags::StringToGpuFeatureType("webgl"),
            GpuFeatureFlags::kGpuFeatureWebgl);
  EXPECT_EQ(GpuFeatureFlags::StringToGpuFeatureType("multisampling"),
            GpuFeatureFlags::kGpuFeatureMultisampling);
  EXPECT_EQ(GpuFeatureFlags::StringToGpuFeatureType("all"),
            GpuFeatureFlags::kGpuFeatureAll);
  EXPECT_EQ(GpuFeatureFlags::StringToGpuFeatureType("xxx"),
            GpuFeatureFlags::kGpuFeatureUnknown);
}

