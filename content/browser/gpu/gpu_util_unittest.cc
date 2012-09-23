// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::GpuFeatureType;

TEST(GpuUtilsTest, GpuFeatureTypFromString) {
  // Test StringToGpuFeatureType.
  EXPECT_EQ(gpu_util::StringToGpuFeatureType("accelerated_2d_canvas"),
            content::GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS);
  EXPECT_EQ(gpu_util::StringToGpuFeatureType("accelerated_compositing"),
            content::GPU_FEATURE_TYPE_ACCELERATED_COMPOSITING);
  EXPECT_EQ(gpu_util::StringToGpuFeatureType("webgl"),
            content::GPU_FEATURE_TYPE_WEBGL);
  EXPECT_EQ(gpu_util::StringToGpuFeatureType("multisampling"),
            content::GPU_FEATURE_TYPE_MULTISAMPLING);
  EXPECT_EQ(gpu_util::StringToGpuFeatureType("flash_3d"),
            content::GPU_FEATURE_TYPE_FLASH3D);
  EXPECT_EQ(gpu_util::StringToGpuFeatureType("flash_stage3d"),
            content::GPU_FEATURE_TYPE_FLASH_STAGE3D);
  EXPECT_EQ(gpu_util::StringToGpuFeatureType("texture_sharing"),
            content::GPU_FEATURE_TYPE_TEXTURE_SHARING);
  EXPECT_EQ(gpu_util::StringToGpuFeatureType("accelerated_video_decode"),
            content::GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE);
  EXPECT_EQ(gpu_util::StringToGpuFeatureType("all"),
            content::GPU_FEATURE_TYPE_ALL);
  EXPECT_EQ(gpu_util::StringToGpuFeatureType("xxx"),
            content::GPU_FEATURE_TYPE_UNKNOWN);
}

TEST(GpuUtilsTest, GpuFeatureTypeToString) {
  // Test GpuFeatureTypeToString for single-bit enums using the all enum
  EXPECT_STREQ(
      gpu_util::GpuFeatureTypeToString(
          content::GPU_FEATURE_TYPE_ACCELERATED_COMPOSITING).c_str(),
      "accelerated_compositing");
  EXPECT_STREQ(
      gpu_util::GpuFeatureTypeToString(
          content::GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS).c_str(),
      "accelerated_2d_canvas");
  EXPECT_STREQ(
      gpu_util::GpuFeatureTypeToString(
          content::GPU_FEATURE_TYPE_WEBGL).c_str(),
      "webgl");
  EXPECT_STREQ(
      gpu_util::GpuFeatureTypeToString(
          content::GPU_FEATURE_TYPE_MULTISAMPLING).c_str(),
      "multisampling");
  EXPECT_STREQ(
      gpu_util::GpuFeatureTypeToString(
          content::GPU_FEATURE_TYPE_FLASH3D).c_str(),
      "flash_3d");
  EXPECT_STREQ(
      gpu_util::GpuFeatureTypeToString(
          content::GPU_FEATURE_TYPE_FLASH_STAGE3D).c_str(),
      "flash_stage3d");
  EXPECT_STREQ(
      gpu_util::GpuFeatureTypeToString(
          content::GPU_FEATURE_TYPE_TEXTURE_SHARING).c_str(),
      "texture_sharing");
  EXPECT_STREQ(
      gpu_util::GpuFeatureTypeToString(
          content::GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE).c_str(),
      "accelerated_video_decode");
  EXPECT_STREQ(
      gpu_util::GpuFeatureTypeToString(
          content::GPU_FEATURE_TYPE_ALL).c_str(),
      "all");
  EXPECT_STREQ(gpu_util::GpuFeatureTypeToString(
          content::GPU_FEATURE_TYPE_UNKNOWN).c_str(),
      "unknown");
  EXPECT_STREQ(
      gpu_util::GpuFeatureTypeToString(
          static_cast<content::GpuFeatureType>(
              content::GPU_FEATURE_TYPE_WEBGL |
              content::GPU_FEATURE_TYPE_MULTISAMPLING)).c_str(),
      "webgl,multisampling");
  EXPECT_STREQ(
      gpu_util::GpuFeatureTypeToString(
          static_cast<content::GpuFeatureType>(
              content::GPU_FEATURE_TYPE_WEBGL |
              content::GPU_FEATURE_TYPE_ALL)).c_str(),
      "all");
}

TEST(GpuUtilsTest, GpuSwitchingOptionFromString) {
  // Test StringToGpuSwitchingOption.
  EXPECT_EQ(gpu_util::StringToGpuSwitchingOption("automatic"),
            content::GPU_SWITCHING_AUTOMATIC);
  EXPECT_EQ(gpu_util::StringToGpuSwitchingOption("force_discrete"),
            content::GPU_SWITCHING_FORCE_DISCRETE);
  EXPECT_EQ(gpu_util::StringToGpuSwitchingOption("force_integrated"),
            content::GPU_SWITCHING_FORCE_INTEGRATED);
  EXPECT_EQ(gpu_util::StringToGpuSwitchingOption("xxx"),
            content::GPU_SWITCHING_UNKNOWN);
}

