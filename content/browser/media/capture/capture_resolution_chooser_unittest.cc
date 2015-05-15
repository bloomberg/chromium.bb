// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/capture/capture_resolution_chooser.h"

#include "base/location.h"
#include "testing/gtest/include/gtest/gtest.h"

using tracked_objects::Location;

namespace content {

namespace {

// 16:9 maximum and minimum frame sizes.
const int kMaxFrameWidth = 3840;
const int kMaxFrameHeight = 2160;
const int kMinFrameWidth = 320;
const int kMinFrameHeight = 180;

// Checks whether |size| is strictly between (inclusive) |min_size| and
// |max_size| and has the same aspect ratio as |max_size|.
void ExpectIsWithinBoundsAndSameAspectRatio(const Location& location,
                                            const gfx::Size& min_size,
                                            const gfx::Size& max_size,
                                            const gfx::Size& size) {
  SCOPED_TRACE(::testing::Message() << "From here: " << location.ToString());
  EXPECT_LE(min_size.width(), size.width());
  EXPECT_LE(min_size.height(), size.height());
  EXPECT_GE(max_size.width(), size.width());
  EXPECT_GE(max_size.height(), size.height());
  EXPECT_NEAR(static_cast<double>(max_size.width()) / max_size.height(),
              static_cast<double>(size.width()) / size.height(),
              0.01);
}

}  // namespace

TEST(CaptureResolutionChooserTest,
     FixedResolutionPolicy_CaptureSizeAlwaysFixed) {
  const gfx::Size the_one_frame_size(kMaxFrameWidth, kMaxFrameHeight);
  CaptureResolutionChooser chooser(the_one_frame_size,
                                   media::RESOLUTION_POLICY_FIXED_RESOLUTION);
  EXPECT_EQ(the_one_frame_size, chooser.capture_size());

  chooser.SetSourceSize(the_one_frame_size);
  EXPECT_EQ(the_one_frame_size, chooser.capture_size());

  chooser.SetSourceSize(gfx::Size(kMaxFrameWidth + 424, kMaxFrameHeight - 101));
  EXPECT_EQ(the_one_frame_size, chooser.capture_size());

  chooser.SetSourceSize(gfx::Size(kMaxFrameWidth - 202, kMaxFrameHeight + 56));
  EXPECT_EQ(the_one_frame_size, chooser.capture_size());

  chooser.SetSourceSize(gfx::Size(kMinFrameWidth, kMinFrameHeight));
  EXPECT_EQ(the_one_frame_size, chooser.capture_size());
}

TEST(CaptureResolutionChooserTest,
     FixedAspectRatioPolicy_CaptureSizeHasSameAspectRatio) {
  CaptureResolutionChooser chooser(
      gfx::Size(kMaxFrameWidth, kMaxFrameHeight),
      media::RESOLUTION_POLICY_FIXED_ASPECT_RATIO);

  // Starting condition.
  const gfx::Size min_size(kMinFrameWidth, kMinFrameHeight);
  const gfx::Size max_size(kMaxFrameWidth, kMaxFrameHeight);
  ExpectIsWithinBoundsAndSameAspectRatio(
      FROM_HERE, min_size, max_size, chooser.capture_size());

  // Max size in --> max size out.
  chooser.SetSourceSize(gfx::Size(kMaxFrameWidth, kMaxFrameHeight));
  ExpectIsWithinBoundsAndSameAspectRatio(
      FROM_HERE, min_size, max_size, chooser.capture_size());

  // Various source sizes within bounds.
  chooser.SetSourceSize(gfx::Size(640, 480));
  ExpectIsWithinBoundsAndSameAspectRatio(
      FROM_HERE, min_size, max_size, chooser.capture_size());

  chooser.SetSourceSize(gfx::Size(480, 640));
  ExpectIsWithinBoundsAndSameAspectRatio(
      FROM_HERE, min_size, max_size, chooser.capture_size());

  chooser.SetSourceSize(gfx::Size(640, 640));
  ExpectIsWithinBoundsAndSameAspectRatio(
      FROM_HERE, min_size, max_size, chooser.capture_size());

  // Bad source size results in no update.
  const gfx::Size unchanged_size = chooser.capture_size();
  chooser.SetSourceSize(gfx::Size(0, 0));
  EXPECT_EQ(unchanged_size, chooser.capture_size());

  // Downscaling size (preserving aspect ratio) when source size exceeds the
  // upper bounds.
  chooser.SetSourceSize(gfx::Size(kMaxFrameWidth * 2, kMaxFrameHeight * 2));
  ExpectIsWithinBoundsAndSameAspectRatio(
      FROM_HERE, min_size, max_size, chooser.capture_size());

  chooser.SetSourceSize(gfx::Size(kMaxFrameWidth * 2, kMaxFrameHeight));
  ExpectIsWithinBoundsAndSameAspectRatio(
      FROM_HERE, min_size, max_size, chooser.capture_size());

  chooser.SetSourceSize(gfx::Size(kMaxFrameWidth, kMaxFrameHeight * 2));
  ExpectIsWithinBoundsAndSameAspectRatio(
      FROM_HERE, min_size, max_size, chooser.capture_size());

  // Upscaling size (preserving aspect ratio) when source size is under the
  // lower bounds.
  chooser.SetSourceSize(gfx::Size(kMinFrameWidth / 2, kMinFrameHeight / 2));
  ExpectIsWithinBoundsAndSameAspectRatio(
      FROM_HERE, min_size, max_size, chooser.capture_size());

  chooser.SetSourceSize(gfx::Size(kMinFrameWidth / 2, kMaxFrameHeight));
  ExpectIsWithinBoundsAndSameAspectRatio(
      FROM_HERE, min_size, max_size, chooser.capture_size());

  chooser.SetSourceSize(gfx::Size(kMinFrameWidth, kMinFrameHeight / 2));
  ExpectIsWithinBoundsAndSameAspectRatio(
      FROM_HERE, min_size, max_size, chooser.capture_size());
}

TEST(CaptureResolutionChooserTest,
     AnyWithinLimitPolicy_CaptureSizeIsAnythingWithinLimits) {
  const gfx::Size max_size(kMaxFrameWidth, kMaxFrameHeight);
  CaptureResolutionChooser chooser(
      max_size, media::RESOLUTION_POLICY_ANY_WITHIN_LIMIT);

  // Starting condition.
  EXPECT_EQ(max_size, chooser.capture_size());

  // Max size in --> max size out.
  chooser.SetSourceSize(max_size);
  EXPECT_EQ(max_size, chooser.capture_size());

  // Various source sizes within bounds.
  chooser.SetSourceSize(gfx::Size(640, 480));
  EXPECT_EQ(gfx::Size(640, 480), chooser.capture_size());

  chooser.SetSourceSize(gfx::Size(480, 640));
  EXPECT_EQ(gfx::Size(480, 640), chooser.capture_size());

  chooser.SetSourceSize(gfx::Size(640, 640));
  EXPECT_EQ(gfx::Size(640, 640), chooser.capture_size());

  chooser.SetSourceSize(gfx::Size(2, 2));
  EXPECT_EQ(gfx::Size(2, 2), chooser.capture_size());

  // Bad source size results in no update.
  const gfx::Size unchanged_size = chooser.capture_size();
  chooser.SetSourceSize(gfx::Size(0, 0));
  EXPECT_EQ(unchanged_size, chooser.capture_size());

  // Downscaling size (preserving aspect ratio) when source size exceeds the
  // upper bounds.
  chooser.SetSourceSize(gfx::Size(kMaxFrameWidth * 2, kMaxFrameHeight * 2));
  EXPECT_EQ(max_size, chooser.capture_size());

  chooser.SetSourceSize(gfx::Size(kMaxFrameWidth * 2, kMaxFrameHeight));
  EXPECT_EQ(gfx::Size(kMaxFrameWidth, kMaxFrameHeight / 2),
            chooser.capture_size());

  chooser.SetSourceSize(gfx::Size(kMaxFrameWidth, kMaxFrameHeight * 2));
  EXPECT_EQ(gfx::Size(kMaxFrameWidth / 2, kMaxFrameHeight),
            chooser.capture_size());
}

}  // namespace content
