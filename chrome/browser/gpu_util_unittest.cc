// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gpu_util.h"
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

  std::string tmp;
  tmp = gpu_util::GpuFeatureTypeToString(
      static_cast<content::GpuFeatureType>(
          content::GPU_FEATURE_TYPE_WEBGL |
          content::GPU_FEATURE_TYPE_ALL));
  EXPECT_STREQ(tmp.c_str(),  "all");
}
