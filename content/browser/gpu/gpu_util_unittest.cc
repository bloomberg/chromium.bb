// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/gpu/gpu_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gl/gl_switches.h"

namespace content {

TEST(GpuUtilsTest, GpuFeatureTypFromString) {
  // Test StringToGpuFeatureType.
  EXPECT_EQ(StringToGpuFeatureType("accelerated_2d_canvas"),
            GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS);
  EXPECT_EQ(StringToGpuFeatureType("accelerated_compositing"),
            GPU_FEATURE_TYPE_ACCELERATED_COMPOSITING);
  EXPECT_EQ(StringToGpuFeatureType("webgl"), GPU_FEATURE_TYPE_WEBGL);
  EXPECT_EQ(StringToGpuFeatureType("multisampling"),
            GPU_FEATURE_TYPE_MULTISAMPLING);
  EXPECT_EQ(StringToGpuFeatureType("flash_3d"), GPU_FEATURE_TYPE_FLASH3D);
  EXPECT_EQ(StringToGpuFeatureType("flash_stage3d"),
            GPU_FEATURE_TYPE_FLASH_STAGE3D);
  EXPECT_EQ(StringToGpuFeatureType("texture_sharing"),
            GPU_FEATURE_TYPE_TEXTURE_SHARING);
  EXPECT_EQ(StringToGpuFeatureType("accelerated_video_decode"),
            GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE);
  EXPECT_EQ(StringToGpuFeatureType("3d_css"), GPU_FEATURE_TYPE_3D_CSS);
  EXPECT_EQ(StringToGpuFeatureType("accelerated_video"),
            GPU_FEATURE_TYPE_ACCELERATED_VIDEO);
  EXPECT_EQ(StringToGpuFeatureType("panel_fitting"),
            GPU_FEATURE_TYPE_PANEL_FITTING);
  EXPECT_EQ(StringToGpuFeatureType("force_compositing_mode"),
            GPU_FEATURE_TYPE_FORCE_COMPOSITING_MODE);
  EXPECT_EQ(StringToGpuFeatureType("all"), GPU_FEATURE_TYPE_ALL);
  EXPECT_EQ(StringToGpuFeatureType("xxx"), GPU_FEATURE_TYPE_UNKNOWN);
}

TEST(GpuUtilsTest, GpuFeatureTypeToString) {
  // Test GpuFeatureTypeToString for single-bit enums using the all enum
  EXPECT_STREQ(
      GpuFeatureTypeToString(GPU_FEATURE_TYPE_ACCELERATED_COMPOSITING).c_str(),
      "accelerated_compositing");
  EXPECT_STREQ(
      GpuFeatureTypeToString(GPU_FEATURE_TYPE_ACCELERATED_2D_CANVAS).c_str(),
      "accelerated_2d_canvas");
  EXPECT_STREQ(
      GpuFeatureTypeToString(GPU_FEATURE_TYPE_WEBGL).c_str(), "webgl");
  EXPECT_STREQ(
      GpuFeatureTypeToString(GPU_FEATURE_TYPE_MULTISAMPLING).c_str(),
      "multisampling");
  EXPECT_STREQ(
      GpuFeatureTypeToString(GPU_FEATURE_TYPE_FLASH3D).c_str(), "flash_3d");
  EXPECT_STREQ(
      GpuFeatureTypeToString(GPU_FEATURE_TYPE_FLASH_STAGE3D).c_str(),
      "flash_stage3d");
  EXPECT_STREQ(
      GpuFeatureTypeToString(GPU_FEATURE_TYPE_TEXTURE_SHARING).c_str(),
      "texture_sharing");
  EXPECT_STREQ(
      GpuFeatureTypeToString(GPU_FEATURE_TYPE_ACCELERATED_VIDEO_DECODE).c_str(),
      "accelerated_video_decode");
  EXPECT_STREQ(
      GpuFeatureTypeToString(GPU_FEATURE_TYPE_3D_CSS).c_str(), "3d_css");
  EXPECT_STREQ(
      GpuFeatureTypeToString(GPU_FEATURE_TYPE_ACCELERATED_VIDEO).c_str(),
      "accelerated_video");
  EXPECT_STREQ(
      GpuFeatureTypeToString(GPU_FEATURE_TYPE_PANEL_FITTING).c_str(),
      "panel_fitting");
  EXPECT_STREQ(
      GpuFeatureTypeToString(GPU_FEATURE_TYPE_FORCE_COMPOSITING_MODE).c_str(),
      "force_compositing_mode");
  EXPECT_STREQ(
      GpuFeatureTypeToString(GPU_FEATURE_TYPE_ALL).c_str(), "all");
  EXPECT_STREQ(GpuFeatureTypeToString(
      GPU_FEATURE_TYPE_UNKNOWN).c_str(), "unknown");
  EXPECT_STREQ(
      GpuFeatureTypeToString(
          static_cast<GpuFeatureType>(
              GPU_FEATURE_TYPE_WEBGL |
              GPU_FEATURE_TYPE_MULTISAMPLING)).c_str(),
      "webgl,multisampling");
  EXPECT_STREQ(
      GpuFeatureTypeToString(
          static_cast<GpuFeatureType>(
              GPU_FEATURE_TYPE_WEBGL |
              GPU_FEATURE_TYPE_ALL)).c_str(),
      "all");
}

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
