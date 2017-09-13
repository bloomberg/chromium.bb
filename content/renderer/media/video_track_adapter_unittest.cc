// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/video_track_adapter.h"

#include <limits>

#include "media/base/limits.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace content {

// Most VideoTrackAdapter functionality is tested in MediaStreamVideoSourceTest.
// These tests focus on the computation of cropped frame sizes in edge cases
// that cannot be easily reproduced by a mocked video source, such as tests
// involving frames of zero size.
// Such frames can be produced by sources in the wild (e.g., element capture).

// Test that cropped sizes with zero-area input frames are correctly computed.
// Aspect ratio limits should be ignored.
TEST(VideoTrackAdapterTest, ZeroInputArea) {
  const double kMinAspectRatio = 0.1;
  const double kMaxAspectRatio = 2.0;
  const int kMaxWidth = 640;
  const int kMaxHeight = 480;
  const bool kIsRotatedValues[] = {true, false};

  for (bool is_rotated : kIsRotatedValues) {
    gfx::Size desired_size;

    VideoTrackAdapter::CalculateTargetSize(
        is_rotated, gfx::Size(0, 0), gfx::Size(kMaxWidth, kMaxHeight),
        kMinAspectRatio, kMaxAspectRatio, &desired_size);
    EXPECT_EQ(desired_size.width(), 0);
    EXPECT_EQ(desired_size.height(), 0);

    // Zero width.
    VideoTrackAdapter::CalculateTargetSize(
        is_rotated, gfx::Size(0, 300), gfx::Size(kMaxWidth, kMaxHeight),
        kMinAspectRatio, kMaxAspectRatio, &desired_size);
    EXPECT_EQ(desired_size.width(), 0);
    EXPECT_EQ(desired_size.height(), 300);

    // Zero height.
    VideoTrackAdapter::CalculateTargetSize(
        is_rotated, gfx::Size(300, 0), gfx::Size(kMaxWidth, kMaxHeight),
        kMinAspectRatio, kMaxAspectRatio, &desired_size);
    EXPECT_EQ(desired_size.width(), 300);
    EXPECT_EQ(desired_size.height(), 0);

    // Requires "cropping" of height.
    VideoTrackAdapter::CalculateTargetSize(
        is_rotated, gfx::Size(0, 1000), gfx::Size(kMaxWidth, kMaxHeight),
        kMinAspectRatio, kMaxAspectRatio, &desired_size);
    EXPECT_EQ(desired_size.width(), 0);
    EXPECT_EQ(desired_size.height(), is_rotated ? kMaxWidth : kMaxHeight);

    // Requires "cropping" of width.
    VideoTrackAdapter::CalculateTargetSize(
        is_rotated, gfx::Size(1000, 0), gfx::Size(kMaxWidth, kMaxHeight),
        kMinAspectRatio, kMaxAspectRatio, &desired_size);
    EXPECT_EQ(desired_size.width(), is_rotated ? kMaxHeight : kMaxWidth);
    EXPECT_EQ(desired_size.height(), 0);
  }
}

// Test that zero-size cropped areas are correctly computed. Aspect ratio
// limits should be ignored.
TEST(VideoTrackAdapterTest, ZeroOutputArea) {
  const double kMinAspectRatio = 0.1;
  const double kMaxAspectRatio = 2.0;
  const int kInputWidth = 640;
  const int kInputHeight = 480;

  gfx::Size desired_size;

  VideoTrackAdapter::CalculateTargetSize(
      false /* is_rotated */, gfx::Size(kInputWidth, kInputHeight),
      gfx::Size(0, 0), kMinAspectRatio, kMaxAspectRatio, &desired_size);
  EXPECT_EQ(desired_size.width(), 0);
  EXPECT_EQ(desired_size.height(), 0);

  // Width is cropped to zero.
  VideoTrackAdapter::CalculateTargetSize(
      false /* is_rotated */, gfx::Size(kInputWidth, kInputHeight),
      gfx::Size(0, 1000), kMinAspectRatio, kMaxAspectRatio, &desired_size);
  EXPECT_EQ(desired_size.width(), 0);
  EXPECT_EQ(desired_size.height(), kInputHeight);

  // Requires "cropping" of width and height.
  VideoTrackAdapter::CalculateTargetSize(
      false /* is_rotated */, gfx::Size(kInputWidth, kInputHeight),
      gfx::Size(0, 300), kMinAspectRatio, kMaxAspectRatio, &desired_size);
  EXPECT_EQ(desired_size.width(), 0);
  EXPECT_EQ(desired_size.height(), 300);

  // Height is cropped to zero.
  VideoTrackAdapter::CalculateTargetSize(
      false /* is_rotated */, gfx::Size(kInputWidth, kInputHeight),
      gfx::Size(1000, 0), kMinAspectRatio, kMaxAspectRatio, &desired_size);
  EXPECT_EQ(desired_size.width(), kInputWidth);
  EXPECT_EQ(desired_size.height(), 0);

  // Requires "cropping" of width and height.
  VideoTrackAdapter::CalculateTargetSize(
      false /* is_rotated */, gfx::Size(kInputWidth, kInputHeight),
      gfx::Size(300, 0), kMinAspectRatio, kMaxAspectRatio, &desired_size);
  EXPECT_EQ(desired_size.width(), 300);
  EXPECT_EQ(desired_size.height(), 0);
}

// Test that large frame sizes are clamped to the maximum supported dimension.
TEST(VideoTrackAdapterTest, ClampToMaxDimension) {
  const double kMinAspectRatio = 0.0;
  const double kMaxAspectRatio = HUGE_VAL;
  const int kInputWidth = std::numeric_limits<int>::max();
  const int kInputHeight = std::numeric_limits<int>::max();
  const int kMaxWidth = std::numeric_limits<int>::max();
  const int kMaxHeight = std::numeric_limits<int>::max();

  gfx::Size desired_size;

  VideoTrackAdapter::CalculateTargetSize(
      false /* is_rotated */, gfx::Size(kInputWidth, kInputHeight),
      gfx::Size(kMaxWidth, kMaxHeight), kMinAspectRatio, kMaxAspectRatio,
      &desired_size);
  EXPECT_EQ(desired_size.width(), media::limits::kMaxDimension);
  EXPECT_EQ(desired_size.height(), media::limits::kMaxDimension);
}

}  // namespace content
