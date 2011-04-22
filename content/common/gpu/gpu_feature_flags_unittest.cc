// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/common/gpu/gpu_feature_flags.h"
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

TEST(GpuFeatureFlagsTest, GpuFeatureTypeToString) {

  // Test GpuFeatureTypeToString for single-bit enums using the all enum
  EXPECT_STREQ(
      GpuFeatureFlags::GpuFeatureTypeToString(
          GpuFeatureFlags::kGpuFeatureAcceleratedCompositing).c_str(),
      "accelerated_compositing");
  EXPECT_STREQ(
      GpuFeatureFlags::GpuFeatureTypeToString(
          GpuFeatureFlags::kGpuFeatureAccelerated2dCanvas).c_str(),
      "accelerated_2d_canvas");
  EXPECT_STREQ(
      GpuFeatureFlags::GpuFeatureTypeToString(
          GpuFeatureFlags::kGpuFeatureWebgl).c_str(),
      "webgl");
  EXPECT_STREQ(
      GpuFeatureFlags::GpuFeatureTypeToString(
          GpuFeatureFlags::kGpuFeatureMultisampling).c_str(),
      "multisampling");
  EXPECT_STREQ(
      GpuFeatureFlags::GpuFeatureTypeToString(
          GpuFeatureFlags::kGpuFeatureAll).c_str(),
      "all");
  EXPECT_STREQ(GpuFeatureFlags::GpuFeatureTypeToString(
          GpuFeatureFlags::kGpuFeatureUnknown).c_str(),
      "unknown");
  EXPECT_STREQ(
      GpuFeatureFlags::GpuFeatureTypeToString(
          static_cast<GpuFeatureFlags::GpuFeatureType>(
              GpuFeatureFlags::kGpuFeatureWebgl |
              GpuFeatureFlags::kGpuFeatureMultisampling)).c_str(),
      "webgl,multisampling");

  std::string tmp;
  tmp = GpuFeatureFlags::GpuFeatureTypeToString(
      static_cast<GpuFeatureFlags::GpuFeatureType>(
          GpuFeatureFlags::kGpuFeatureWebgl |
          GpuFeatureFlags::kGpuFeatureAll));
  EXPECT_STREQ(tmp.c_str(),  "all");
}
