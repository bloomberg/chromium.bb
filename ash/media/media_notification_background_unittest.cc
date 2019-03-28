// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/media/media_notification_background.h"

#include <memory>

#include "ash/test/ash_test_base.h"
#include "ui/views/test/test_views.h"

namespace ash {

class MediaNotificationBackgroundTest : public AshTestBase {
 public:
  MediaNotificationBackgroundTest() = default;
  ~MediaNotificationBackgroundTest() override = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(MediaNotificationBackgroundTest);
};

TEST_F(MediaNotificationBackgroundTest, BoundsSanityCheck) {
  views::StaticSizedView owner;
  MediaNotificationBackground background(&owner, 10, 10, 0.1);

  // The test notification will have a width of 200 and a height of 50.
  gfx::Rect bounds(0, 0, 200, 50);

  // Check the artwork is not visible by default.
  EXPECT_EQ(0, background.GetArtworkWidth(bounds.size()));
  EXPECT_EQ(0, background.GetArtworkVisibleWidth(bounds.size()));
  EXPECT_EQ(gfx::Rect(200, 0, 0, 50), background.GetArtworkBounds(bounds));
  EXPECT_EQ(gfx::Rect(0, 0, 200, 50),
            background.GetFilledBackgroundBounds(bounds));
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), background.GetGradientBounds(bounds));

  // The background artwork image will have an aspect ratio of 2:1.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(20, 10);
  background.UpdateArtwork(gfx::ImageSkia::CreateFrom1xBitmap(bitmap));

  // The artwork width will be 2x the height of the notification and the visible
  // width will be limited to 10% the width of the notification.
  EXPECT_EQ(100, background.GetArtworkWidth(bounds.size()));
  EXPECT_EQ(20, background.GetArtworkVisibleWidth(bounds.size()));

  // Update the visible width % to be greater than the width of the image.
  background.UpdateArtworkMaxWidthPct(0.6);
  EXPECT_EQ(100, background.GetArtworkVisibleWidth(bounds.size()));

  // Check the artwork is positioned to the right.
  EXPECT_EQ(gfx::Rect(100, 0, 100, 50), background.GetArtworkBounds(bounds));

  // Check the filled background is to the left of the image.
  EXPECT_EQ(gfx::Rect(0, 0, 100, 50),
            background.GetFilledBackgroundBounds(bounds));

  // Check the gradient is positioned above the artwork.
  EXPECT_EQ(gfx::Rect(100, 0, 40, 50), background.GetGradientBounds(bounds));
}

}  // namespace ash
