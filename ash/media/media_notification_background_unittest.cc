// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/media/media_notification_background.h"

#include <memory>

#include "ash/test/ash_test_base.h"
#include "base/i18n/base_i18n_switches.h"
#include "base/test/icu_test_util.h"
#include "base/test/scoped_command_line.h"
#include "ui/views/test/test_views.h"

namespace ash {

namespace {

gfx::ImageSkia CreateTestBackgroundImage(SkColor first_color,
                                         SkColor second_color,
                                         int second_height) {
  constexpr SkColor kRightHandSideColor = SK_ColorMAGENTA;

  DCHECK_NE(kRightHandSideColor, first_color);
  DCHECK_NE(kRightHandSideColor, second_color);

  SkBitmap bitmap;
  bitmap.allocN32Pixels(100, 100);

  int first_height = bitmap.height() - second_height;
  int right_width = bitmap.width() / 2;

  // Fill the right hand side of the image with a constant color. The color
  // derivation algorithm does not look at the right hand side so we should
  // never see |kRightHandSideColor|.
  bitmap.erase(kRightHandSideColor,
               {right_width, 0, bitmap.width(), bitmap.height()});

  // Fill the left hand side with |first_color|.
  bitmap.erase(first_color, {0, 0, right_width, first_height});

  // Fill the left hand side with |second_color|.
  bitmap.erase(second_color, {0, first_height, right_width, bitmap.height()});

  return gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
}

gfx::ImageSkia CreateTestBackgroundImage(SkColor color) {
  return CreateTestBackgroundImage(color, SK_ColorTRANSPARENT, 0);
}

}  // namespace

class MediaNotificationBackgroundTest : public AshTestBase {
 public:
  MediaNotificationBackgroundTest() = default;
  ~MediaNotificationBackgroundTest() override = default;

  void SetUp() override {
    AshTestBase::SetUp();

    owner_ = std::make_unique<views::StaticSizedView>();
    background_ = std::make_unique<MediaNotificationBackground>(owner_.get(),
                                                                10, 10, 0.1);

    EXPECT_FALSE(GetBackgroundColor().has_value());
  }

  void TearDown() override {
    background_.reset();
    owner_.reset();

    AshTestBase::TearDown();
  }

  MediaNotificationBackground* background() const { return background_.get(); }

  base::Optional<SkColor> GetBackgroundColor() const {
    return background_->background_color_;
  }

 private:
  std::unique_ptr<views::StaticSizedView> owner_;
  std::unique_ptr<MediaNotificationBackground> background_;

  DISALLOW_COPY_AND_ASSIGN(MediaNotificationBackgroundTest);
};

// If we have no artwork then we should use the default background color.
TEST_F(MediaNotificationBackgroundTest, DeriveBackgroundColor_NoArtwork) {
  background()->UpdateArtwork(gfx::ImageSkia());
  EXPECT_FALSE(GetBackgroundColor().has_value());
}

// If we have artwork with no popular color then we should use the default
// background color.
TEST_F(MediaNotificationBackgroundTest, DeriveBackgroundColor_NoPopularColor) {
  background()->UpdateArtwork(CreateTestBackgroundImage(SK_ColorTRANSPARENT));
  EXPECT_FALSE(GetBackgroundColor().has_value());
}

// If the most popular color is not white or black then we should use that
// color.
TEST_F(MediaNotificationBackgroundTest,
       DeriveBackgroundColor_PopularNonWhiteBlackColor) {
  constexpr SkColor kTestColor = SK_ColorYELLOW;
  background()->UpdateArtwork(CreateTestBackgroundImage(kTestColor));
  EXPECT_EQ(kTestColor, GetBackgroundColor());
}

// MediaNotificationBackgroundBlackWhiteTest will repeat these tests with a
// parameter that is either black or white.
class MediaNotificationBackgroundBlackWhiteTest
    : public MediaNotificationBackgroundTest,
      public testing::WithParamInterface<SkColor> {};

INSTANTIATE_TEST_SUITE_P(,
                         MediaNotificationBackgroundBlackWhiteTest,
                         testing::Values(SK_ColorBLACK, SK_ColorWHITE));

// If the most popular color is black or white but there is no secondary color
// we should use the most popular color.
TEST_P(MediaNotificationBackgroundBlackWhiteTest,
       DeriveBackgroundColor_PopularBlackWhiteNoSecondaryColor) {
  background()->UpdateArtwork(CreateTestBackgroundImage(GetParam()));
  EXPECT_EQ(GetParam(), GetBackgroundColor());
}

// If the most popular color is black or white and there is a secondary color
// that is very minor then we should use the most popular color.
TEST_P(MediaNotificationBackgroundBlackWhiteTest,
       DeriveBackgroundColor_VeryPopularBlackWhite) {
  background()->UpdateArtwork(
      CreateTestBackgroundImage(GetParam(), SK_ColorYELLOW, 20));
  EXPECT_EQ(GetParam(), GetBackgroundColor());
}

// If the most popular color is black or white but it is not that popular then
// we should use the secondary color.
TEST_P(MediaNotificationBackgroundBlackWhiteTest,
       DeriveBackgroundColor_NotVeryPopularBlackWhite) {
  constexpr SkColor kTestColor = SK_ColorYELLOW;
  background()->UpdateArtwork(
      CreateTestBackgroundImage(GetParam(), kTestColor, 40));
  EXPECT_EQ(kTestColor, GetBackgroundColor());
}

// MediaNotificationBackgroundRTLTest will repeat these tests with RTL disabled
// and enabled.
class MediaNotificationBackgroundRTLTest
    : public MediaNotificationBackgroundTest,
      public testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        switches::kForceUIDirection, GetParam() ? switches::kForceDirectionRTL
                                                : switches::kForceDirectionLTR);

    MediaNotificationBackgroundTest::SetUp();
  }

  bool IsRTL() const { return GetParam(); }

 private:
  base::test::ScopedRestoreICUDefaultLocale scoped_locale_;
  base::test::ScopedCommandLine command_line_;
};

INSTANTIATE_TEST_SUITE_P(, MediaNotificationBackgroundRTLTest, testing::Bool());

TEST_P(MediaNotificationBackgroundRTLTest, BoundsSanityCheck) {
  // The test notification will have a width of 200 and a height of 50.
  gfx::Rect bounds(0, 0, 200, 50);

  // Check the artwork is not visible by default.
  EXPECT_EQ(0, background()->GetArtworkWidth(bounds.size()));
  EXPECT_EQ(0, background()->GetArtworkVisibleWidth(bounds.size()));
  EXPECT_EQ(gfx::Rect(IsRTL() ? -200 : 200, 0, 0, 50),
            background()->GetArtworkBounds(bounds));
  EXPECT_EQ(gfx::Rect(IsRTL() ? -200 : 0, 0, 200, 50),
            background()->GetFilledBackgroundBounds(bounds));
  EXPECT_EQ(gfx::Rect(0, 0, 0, 0), background()->GetGradientBounds(bounds));

  // The background artwork image will have an aspect ratio of 2:1.
  SkBitmap bitmap;
  bitmap.allocN32Pixels(20, 10);
  bitmap.eraseColor(SK_ColorWHITE);
  background()->UpdateArtwork(gfx::ImageSkia::CreateFrom1xBitmap(bitmap));

  // The artwork width will be 2x the height of the notification and the visible
  // width will be limited to 10% the width of the notification.
  EXPECT_EQ(100, background()->GetArtworkWidth(bounds.size()));
  EXPECT_EQ(20, background()->GetArtworkVisibleWidth(bounds.size()));

  // Update the visible width % to be greater than the width of the image.
  background()->UpdateArtworkMaxWidthPct(0.6);
  EXPECT_EQ(100, background()->GetArtworkVisibleWidth(bounds.size()));

  // Check the artwork is positioned to the right.
  EXPECT_EQ(gfx::Rect(IsRTL() ? -200 : 100, 0, 100, 50),
            background()->GetArtworkBounds(bounds));

  // Check the filled background is to the left of the image.
  EXPECT_EQ(gfx::Rect(IsRTL() ? -100 : 0, 0, 100, 50),
            background()->GetFilledBackgroundBounds(bounds));

  // Check the gradient is positioned above the artwork.
  const gfx::Rect gradient_bounds = background()->GetGradientBounds(bounds);
  EXPECT_EQ(gfx::Rect(IsRTL() ? -140 : 100, 0, 40, 50), gradient_bounds);

  // Check the gradient point X-values are the start and end of
  // |gradient_bounds|.
  EXPECT_EQ(IsRTL() ? -100 : 100,
            background()->GetGradientStartPoint(gradient_bounds).x());
  EXPECT_EQ(IsRTL() ? -140 : 140,
            background()->GetGradientEndPoint(gradient_bounds).x());

  // Check both of the gradient point Y-values are half the height.
  EXPECT_EQ(25, background()->GetGradientStartPoint(gradient_bounds).y());
  EXPECT_EQ(25, background()->GetGradientEndPoint(gradient_bounds).y());
}

}  // namespace ash
