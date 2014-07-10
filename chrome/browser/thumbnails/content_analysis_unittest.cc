// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/thumbnails/content_analysis.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <limits>
#include <numeric>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/image/image.h"
#include "ui/gfx/rect.h"
#include "ui/gfx/size.h"

namespace {

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

unsigned long ImagePixelSum(const SkBitmap& bitmap, const gfx::Rect& rect) {
  // Get the sum of pixel values in the rectangle. Applicable only to
  // monochrome bitmaps.
  DCHECK_EQ(kAlpha_8_SkColorType, bitmap.colorType());
  unsigned long total = 0;
  for (int r = rect.y(); r < rect.bottom(); ++r) {
    const uint8* row_data = static_cast<const uint8*>(
        bitmap.getPixels()) + r * bitmap.rowBytes();
    for (int c = rect.x(); c < rect.right(); ++c)
      total += row_data[c];
  }

  return total;
}

bool CompareImageFragments(const SkBitmap& bitmap_left,
                           const SkBitmap& bitmap_right,
                           const gfx::Size& comparison_area,
                           const gfx::Point& origin_left,
                           const gfx::Point& origin_right) {
  SkAutoLockPixels left_lock(bitmap_left);
  SkAutoLockPixels right_lock(bitmap_right);
  for (int r = 0; r < comparison_area.height(); ++r) {
    for (int c = 0; c < comparison_area.width(); ++c) {
      SkColor color_left = bitmap_left.getColor(origin_left.x() + c,
                                                origin_left.y() + r);
      SkColor color_right = bitmap_right.getColor(origin_right.x() + c,
                                                  origin_right.y() + r);
      if (color_left != color_right)
        return false;
    }
  }

  return true;
}

float AspectDifference(const gfx::Size& reference, const gfx::Size& candidate) {
  return std::abs(static_cast<float>(candidate.width()) / candidate.height() -
                  static_cast<float>(reference.width()) / reference.height());
}

}  // namespace

namespace thumbnailing_utils {

class ThumbnailContentAnalysisTest : public testing::Test {
};

TEST_F(ThumbnailContentAnalysisTest, ApplyGradientMagnitudeOnImpulse) {
  gfx::Canvas canvas(gfx::Size(800, 600), 1.0f, true);

  // The image consists of a point spike on uniform (non-zero) background.
  canvas.FillRect(gfx::Rect(0, 0, 800, 600), SkColorSetRGB(10, 10, 10));
  canvas.FillRect(gfx::Rect(400, 300, 1, 1), SkColorSetRGB(255, 255, 255));

  SkBitmap source =
      skia::GetTopDevice(*canvas.sk_canvas())->accessBitmap(false);

  SkBitmap reduced_color;
  reduced_color.allocPixels(SkImageInfo::MakeA8(source.width(),
                                                source.height()));

  gfx::Vector3dF transform(0.299f, 0.587f, 0.114f);
  EXPECT_TRUE(color_utils::ApplyColorReduction(
      source, transform, true, &reduced_color));

  float sigma = 2.5f;
  ApplyGaussianGradientMagnitudeFilter(&reduced_color, sigma);

  // Expect everything to be within 8 * sigma.
  int tail_length = static_cast<int>(8.0f * sigma + 0.5f);
  gfx::Rect echo_rect(399 - tail_length, 299 - tail_length,
                      2 * tail_length + 1, 2 * tail_length + 1);
  unsigned long data_sum = ImagePixelSum(reduced_color, echo_rect);
  unsigned long all_sum = ImagePixelSum(reduced_color, gfx::Rect(800, 600));
  EXPECT_GT(data_sum, 0U);
  EXPECT_EQ(data_sum, all_sum);

  sigma = 5.0f;
  ApplyGaussianGradientMagnitudeFilter(&reduced_color, sigma);

  // Expect everything to be within 8 * sigma.
  tail_length = static_cast<int>(8.0f * sigma + 0.5f);
  echo_rect = gfx::Rect(399 - tail_length, 299 - tail_length,
                        2 * tail_length + 1, 2 * tail_length + 1);
  data_sum = ImagePixelSum(reduced_color, echo_rect);
  all_sum = ImagePixelSum(reduced_color, gfx::Rect(800, 600));
  EXPECT_GT(data_sum, 0U);
  EXPECT_EQ(data_sum, all_sum);
}

TEST_F(ThumbnailContentAnalysisTest, ApplyGradientMagnitudeOnFrame) {
  gfx::Canvas canvas(gfx::Size(800, 600), 1.0f, true);

  // The image consists of a single white block in the centre.
  gfx::Rect draw_rect(300, 200, 200, 200);
  canvas.FillRect(gfx::Rect(0, 0, 800, 600), SkColorSetRGB(0, 0, 0));
  canvas.DrawRect(draw_rect, SkColorSetRGB(255, 255, 255));

  SkBitmap source =
      skia::GetTopDevice(*canvas.sk_canvas())->accessBitmap(false);

  SkBitmap reduced_color;
  reduced_color.allocPixels(SkImageInfo::MakeA8(source.width(),
                                                source.height()));

  gfx::Vector3dF transform(0.299f, 0.587f, 0.114f);
  EXPECT_TRUE(color_utils::ApplyColorReduction(
      source, transform, true, &reduced_color));

  float sigma = 2.5f;
  ApplyGaussianGradientMagnitudeFilter(&reduced_color, sigma);

  int tail_length = static_cast<int>(8.0f * sigma + 0.5f);
  gfx::Rect outer_rect(draw_rect.x() - tail_length,
                       draw_rect.y() - tail_length,
                       draw_rect.width() + 2 * tail_length,
                       draw_rect.height() + 2 * tail_length);
  gfx::Rect inner_rect(draw_rect.x() + tail_length,
                       draw_rect.y() + tail_length,
                       draw_rect.width() - 2 * tail_length,
                       draw_rect.height() - 2 * tail_length);
  unsigned long data_sum = ImagePixelSum(reduced_color, outer_rect);
  unsigned long all_sum = ImagePixelSum(reduced_color, gfx::Rect(800, 600));
  EXPECT_GT(data_sum, 0U);
  EXPECT_EQ(data_sum, all_sum);
  EXPECT_EQ(ImagePixelSum(reduced_color, inner_rect), 0U);
}

TEST_F(ThumbnailContentAnalysisTest, ExtractImageProfileInformation) {
  gfx::Canvas canvas(gfx::Size(800, 600), 1.0f, true);

  // The image consists of a white frame drawn in the centre.
  gfx::Rect draw_rect(100, 100, 200, 100);
  gfx::Rect image_rect(0, 0, 800, 600);
  canvas.FillRect(image_rect, SkColorSetRGB(0, 0, 0));
  canvas.DrawRect(draw_rect, SkColorSetRGB(255, 255, 255));

  SkBitmap source =
      skia::GetTopDevice(*canvas.sk_canvas())->accessBitmap(false);
  SkBitmap reduced_color;
  reduced_color.allocPixels(SkImageInfo::MakeA8(source.width(),
                                                source.height()));

  gfx::Vector3dF transform(1, 0, 0);
  EXPECT_TRUE(color_utils::ApplyColorReduction(
      source, transform, true, &reduced_color));
  std::vector<float> column_profile;
  std::vector<float> row_profile;
  ExtractImageProfileInformation(reduced_color,
                                 image_rect,
                                 gfx::Size(),
                                 false,
                                 &row_profile,
                                 &column_profile);
  EXPECT_EQ(0, std::accumulate(column_profile.begin(),
                               column_profile.begin() + draw_rect.x() - 1,
                               0));
  EXPECT_EQ(column_profile[draw_rect.x()], 255U * (draw_rect.height() + 1));
  EXPECT_EQ(2 * 255 * (draw_rect.width() - 2),
            std::accumulate(column_profile.begin() + draw_rect.x() + 1,
                            column_profile.begin() + draw_rect.right() - 1,
                            0));

  EXPECT_EQ(0, std::accumulate(row_profile.begin(),
                               row_profile.begin() + draw_rect.y() - 1,
                               0));
  EXPECT_EQ(row_profile[draw_rect.y()], 255U * (draw_rect.width() + 1));
  EXPECT_EQ(2 * 255 * (draw_rect.height() - 2),
            std::accumulate(row_profile.begin() + draw_rect.y() + 1,
                            row_profile.begin() + draw_rect.bottom() - 1,
                            0));

  gfx::Rect test_rect(150, 80, 400, 100);
  ExtractImageProfileInformation(reduced_color,
                                 test_rect,
                                 gfx::Size(),
                                 false,
                                 &row_profile,
                                 &column_profile);

  // Some overlap with the drawn rectagle. If you work it out on a piece of
  // paper, sums should be as follows.
  EXPECT_EQ(255 * (test_rect.bottom() - draw_rect.y()) +
            255 * (draw_rect.right() - test_rect.x()),
            std::accumulate(row_profile.begin(), row_profile.end(), 0));
  EXPECT_EQ(255 * (test_rect.bottom() - draw_rect.y()) +
            255 * (draw_rect.right() - test_rect.x()),
            std::accumulate(column_profile.begin(), column_profile.end(), 0));
}

TEST_F(ThumbnailContentAnalysisTest,
       ExtractImageProfileInformationWithClosing) {
  gfx::Canvas canvas(gfx::Size(800, 600), 1.0f, true);

  // The image consists of a two white frames drawn side by side, with a
  // single-pixel vertical gap in between.
  gfx::Rect image_rect(0, 0, 800, 600);
  canvas.FillRect(image_rect, SkColorSetRGB(0, 0, 0));
  canvas.DrawRect(gfx::Rect(300, 250, 99, 100), SkColorSetRGB(255, 255, 255));
  canvas.DrawRect(gfx::Rect(401, 250, 99, 100), SkColorSetRGB(255, 255, 255));

  SkBitmap source =
      skia::GetTopDevice(*canvas.sk_canvas())->accessBitmap(false);
  SkBitmap reduced_color;
  reduced_color.allocPixels(SkImageInfo::MakeA8(source.width(),
                                                source.height()));

  gfx::Vector3dF transform(1, 0, 0);
  EXPECT_TRUE(color_utils::ApplyColorReduction(
      source, transform, true, &reduced_color));
  std::vector<float> column_profile;
  std::vector<float> row_profile;

  ExtractImageProfileInformation(reduced_color,
                                 image_rect,
                                 gfx::Size(),
                                 true,
                                 &row_profile,
                                 &column_profile);
  // Column profiles should have two spikes in the middle, with a single
  // 0-valued value between them.
  EXPECT_GT(column_profile[398], 0.0f);
  EXPECT_GT(column_profile[399], column_profile[398]);
  EXPECT_GT(column_profile[402], 0.0f);
  EXPECT_GT(column_profile[401], column_profile[402]);
  EXPECT_EQ(column_profile[401], column_profile[399]);
  EXPECT_EQ(column_profile[402], column_profile[398]);
  EXPECT_EQ(column_profile[400], 0.0f);
  EXPECT_EQ(column_profile[299], 0.0f);
  EXPECT_EQ(column_profile[502], 0.0f);

  // Now the same with closing applied. The space in the middle will be closed.
  ExtractImageProfileInformation(reduced_color,
                                 image_rect,
                                 gfx::Size(200, 100),
                                 true,
                                 &row_profile,
                                 &column_profile);
  EXPECT_GT(column_profile[398], 0);
  EXPECT_GT(column_profile[400], 0);
  EXPECT_GT(column_profile[402], 0);
  EXPECT_EQ(column_profile[299], 0);
  EXPECT_EQ(column_profile[502], 0);
  EXPECT_EQ(column_profile[399], column_profile[401]);
  EXPECT_EQ(column_profile[398], column_profile[402]);
}

TEST_F(ThumbnailContentAnalysisTest, AdjustClippingSizeToAspectRatio) {
  // The test will exercise several relations of sizes. Basic invariants
  // checked in each case: each dimension in adjusted_size ougth not be greater
  // than the source image and not lesser than requested target. Aspect ratio
  // of adjusted_size should never be worse than that of computed_size.
  gfx::Size target_size(212, 100);
  gfx::Size image_size(1000, 2000);
  gfx::Size computed_size(420, 200);

  gfx::Size adjusted_size = AdjustClippingSizeToAspectRatio(
      target_size, image_size, computed_size);

  EXPECT_LE(adjusted_size.width(), image_size.width());
  EXPECT_LE(adjusted_size.height(), image_size.height());
  EXPECT_GE(adjusted_size.width(), target_size.width());
  EXPECT_GE(adjusted_size.height(), target_size.height());
  EXPECT_LE(AspectDifference(target_size, adjusted_size),
            AspectDifference(target_size, computed_size));
  // This case is special (and trivial): no change expected.
  EXPECT_EQ(computed_size, adjusted_size);

  // Computed size is too tall. Adjusted size has to add rows.
  computed_size.SetSize(600, 150);
  adjusted_size = AdjustClippingSizeToAspectRatio(
      target_size, image_size, computed_size);
  // Invariant check.
  EXPECT_LE(adjusted_size.width(), image_size.width());
  EXPECT_LE(adjusted_size.height(), image_size.height());
  EXPECT_GE(adjusted_size.width(), target_size.width());
  EXPECT_GE(adjusted_size.height(), target_size.height());
  EXPECT_LE(AspectDifference(target_size, adjusted_size),
            AspectDifference(target_size, computed_size));
  // Specific to this case.
  EXPECT_EQ(computed_size.width(), adjusted_size.width());
  EXPECT_LE(computed_size.height(), adjusted_size.height());
  EXPECT_NEAR(
      static_cast<float>(target_size.width()) / target_size.height(),
      static_cast<float>(adjusted_size.width()) / adjusted_size.height(),
      0.02f);

  // Computed size is too wide. Adjusted size has to add columns.
  computed_size.SetSize(200, 400);
  adjusted_size = AdjustClippingSizeToAspectRatio(
      target_size, image_size, computed_size);
  // Invariant check.
  EXPECT_LE(adjusted_size.width(), image_size.width());
  EXPECT_LE(adjusted_size.height(), image_size.height());
  EXPECT_GE(adjusted_size.width(), target_size.width());
  EXPECT_GE(adjusted_size.height(), target_size.height());
  EXPECT_LE(AspectDifference(target_size, adjusted_size),
            AspectDifference(target_size, computed_size));
  EXPECT_NEAR(
      static_cast<float>(target_size.width()) / target_size.height(),
      static_cast<float>(adjusted_size.width()) / adjusted_size.height(),
      0.02f);

  target_size.SetSize(416, 205);
  image_size.SetSize(1200, 1200);
  computed_size.SetSize(900, 300);
  adjusted_size = AdjustClippingSizeToAspectRatio(
      target_size, image_size, computed_size);
  // Invariant check.
  EXPECT_LE(adjusted_size.width(), image_size.width());
  EXPECT_LE(adjusted_size.height(), image_size.height());
  EXPECT_GE(adjusted_size.width(), target_size.width());
  EXPECT_GE(adjusted_size.height(), target_size.height());
  EXPECT_LE(AspectDifference(target_size, adjusted_size),
            AspectDifference(target_size, computed_size));
  // Specific to this case.
  EXPECT_EQ(computed_size.width(), adjusted_size.width());
  EXPECT_LE(computed_size.height(), adjusted_size.height());
  EXPECT_NEAR(
      static_cast<float>(target_size.width()) / target_size.height(),
      static_cast<float>(adjusted_size.width()) / adjusted_size.height(),
      0.02f);

  target_size.SetSize(416, 205);
  image_size.SetSize(1200, 1200);
  computed_size.SetSize(300, 300);
  adjusted_size = AdjustClippingSizeToAspectRatio(
      target_size, image_size, computed_size);
  // Invariant check.
  EXPECT_LE(adjusted_size.width(), image_size.width());
  EXPECT_LE(adjusted_size.height(), image_size.height());
  EXPECT_GE(adjusted_size.width(), target_size.width());
  EXPECT_GE(adjusted_size.height(), target_size.height());
  EXPECT_LE(AspectDifference(target_size, adjusted_size),
            AspectDifference(target_size, computed_size));
  // Specific to this case.
  EXPECT_EQ(computed_size.height(), adjusted_size.height());
  EXPECT_LE(computed_size.width(), adjusted_size.width());
  EXPECT_NEAR(
      static_cast<float>(target_size.width()) / target_size.height(),
      static_cast<float>(adjusted_size.width()) / adjusted_size.height(),
      0.02f);

  computed_size.SetSize(200, 300);
  adjusted_size = AdjustClippingSizeToAspectRatio(
      target_size, image_size, computed_size);
  // Invariant check.
  EXPECT_LE(adjusted_size.width(), image_size.width());
  EXPECT_LE(adjusted_size.height(), image_size.height());
  EXPECT_GE(adjusted_size.width(), target_size.width());
  EXPECT_GE(adjusted_size.height(), target_size.height());
  EXPECT_LE(AspectDifference(target_size, adjusted_size),
            AspectDifference(target_size, computed_size));
  // Specific to this case.
  EXPECT_EQ(computed_size.height(), adjusted_size.height());
  EXPECT_LE(computed_size.width(), adjusted_size.width());
  EXPECT_NEAR(
      static_cast<float>(target_size.width()) / target_size.height(),
      static_cast<float>(adjusted_size.width()) / adjusted_size.height(),
      0.02f);

  target_size.SetSize(416, 205);
  image_size.SetSize(1400, 600);
  computed_size.SetSize(300, 300);
  adjusted_size = AdjustClippingSizeToAspectRatio(
      target_size, image_size, computed_size);
  // Invariant check.
  EXPECT_LE(adjusted_size.width(), image_size.width());
  EXPECT_LE(adjusted_size.height(), image_size.height());
  EXPECT_GE(adjusted_size.width(), target_size.width());
  EXPECT_GE(adjusted_size.height(), target_size.height());
  EXPECT_LE(AspectDifference(target_size, adjusted_size),
            AspectDifference(target_size, computed_size));
  // Specific to this case.
  EXPECT_EQ(computed_size.height(), adjusted_size.height());
  EXPECT_LE(computed_size.width(), adjusted_size.width());
  EXPECT_NEAR(
      static_cast<float>(target_size.width()) / target_size.height(),
      static_cast<float>(adjusted_size.width()) / adjusted_size.height(),
      0.02f);

  computed_size.SetSize(900, 300);
  adjusted_size = AdjustClippingSizeToAspectRatio(
      target_size, image_size, computed_size);
  // Invariant check.
  EXPECT_LE(adjusted_size.width(), image_size.width());
  EXPECT_LE(adjusted_size.height(), image_size.height());
  EXPECT_GE(adjusted_size.width(), target_size.width());
  EXPECT_GE(adjusted_size.height(), target_size.height());
  EXPECT_LE(AspectDifference(target_size, adjusted_size),
            AspectDifference(target_size, computed_size));
  // Specific to this case.
  EXPECT_LE(computed_size.height(), adjusted_size.height());
  EXPECT_NEAR(
      static_cast<float>(target_size.width()) / target_size.height(),
      static_cast<float>(adjusted_size.width()) / adjusted_size.height(),
      0.02f);
}

TEST_F(ThumbnailContentAnalysisTest, AutoSegmentPeaks) {
  std::vector<float> profile_info;

  EXPECT_EQ(AutoSegmentPeaks(profile_info), std::numeric_limits<float>::max());
  profile_info.resize(1000, 1.0f);
  EXPECT_EQ(AutoSegmentPeaks(profile_info), 1.0f);
  std::srand(42);
  std::generate(profile_info.begin(), profile_info.end(), std::rand);
  float threshold = AutoSegmentPeaks(profile_info);
  EXPECT_GT(threshold, 0);  // Not much to expect.

  // There should be roughly 50% above and below the threshold.
  // Random is not really random thanks to srand, so we can sort-of compare.
  int above_count = std::count_if(
      profile_info.begin(),
      profile_info.end(),
      std::bind2nd(std::greater<float>(), threshold));
  EXPECT_GT(above_count, 450);  // Not much to expect.
  EXPECT_LT(above_count, 550);

  for (unsigned i = 0; i < profile_info.size(); ++i) {
    float y = std::sin(M_PI * i / 250.0f);
    profile_info[i] = y > 0 ? y : 0;
  }
  threshold = AutoSegmentPeaks(profile_info);

  above_count = std::count_if(
      profile_info.begin(),
      profile_info.end(),
      std::bind2nd(std::greater<float>(), threshold));
  EXPECT_LT(above_count, 500);  // Negative y expected to fall below threshold.

  // Expect two peaks around between 0 and 250 and 500 and 750.
  std::vector<bool> thresholded_values(profile_info.size(), false);
  std::transform(profile_info.begin(),
                 profile_info.end(),
                 thresholded_values.begin(),
                 std::bind2nd(std::greater<float>(), threshold));
  EXPECT_TRUE(thresholded_values[125]);
  EXPECT_TRUE(thresholded_values[625]);
  int transitions = 0;
  for (unsigned i = 1; i < thresholded_values.size(); ++i) {
    if (thresholded_values[i] != thresholded_values[i-1])
      transitions++;
  }
  EXPECT_EQ(transitions, 4);  // We have two contiguous peaks. Good going!
}

TEST_F(ThumbnailContentAnalysisTest, ConstrainedProfileSegmentation) {
  const size_t kRowCount = 800;
  const size_t kColumnCount = 1400;
  const gfx::Size target_size(300, 150);
  std::vector<float> rows_profile(kRowCount);
  std::vector<float> columns_profile(kColumnCount);

  std::srand(42);
  std::generate(rows_profile.begin(), rows_profile.end(), std::rand);
  std::generate(columns_profile.begin(), columns_profile.end(), std::rand);

  // Bring noise level to 0-1.
  std::transform(rows_profile.begin(),
                 rows_profile.end(),
                 rows_profile.begin(),
                 std::bind2nd(std::divides<float>(), RAND_MAX));
  std::transform(columns_profile.begin(),
                 columns_profile.end(),
                 columns_profile.begin(),
                 std::bind2nd(std::divides<float>(), RAND_MAX));

  // Set up values to 0-1.
  std::transform(rows_profile.begin(),
                 rows_profile.end(),
                 rows_profile.begin(),
                 std::bind2nd(std::plus<float>(), 1.0f));
  std::transform(columns_profile.begin(),
                 columns_profile.end(),
                 columns_profile.begin(),
                 std::bind2nd(std::plus<float>(), 1.0f));

  std::transform(rows_profile.begin() + 300,
                 rows_profile.begin() + 450,
                 rows_profile.begin() + 300,
                 std::bind2nd(std::plus<float>(), 8.0f));
  std::transform(columns_profile.begin() + 400,
                 columns_profile.begin() + 1000,
                 columns_profile.begin() + 400,
                 std::bind2nd(std::plus<float>(), 10.0f));

  // Make sure that threshold falls somewhere reasonable.
  float row_threshold = AutoSegmentPeaks(rows_profile);
  EXPECT_GT(row_threshold, 1.0f);
  EXPECT_LT(row_threshold, 9.0f);

  int row_above_count = std::count_if(
      rows_profile.begin(),
      rows_profile.end(),
      std::bind2nd(std::greater<float>(), row_threshold));
  EXPECT_EQ(row_above_count, 150);

  float column_threshold = AutoSegmentPeaks(columns_profile);
  EXPECT_GT(column_threshold, 1.0f);
  EXPECT_LT(column_threshold, 11.0f);

  int column_above_count = std::count_if(
      columns_profile.begin(),
      columns_profile.end(),
      std::bind2nd(std::greater<float>(), column_threshold));
  EXPECT_EQ(column_above_count, 600);


  std::vector<bool> rows_guide;
  std::vector<bool> columns_guide;
  ConstrainedProfileSegmentation(
      rows_profile, columns_profile, target_size, &rows_guide, &columns_guide);

  int row_count = std::count(rows_guide.begin(), rows_guide.end(), true);
  int column_count = std::count(
      columns_guide.begin(), columns_guide.end(), true);
  float expected_aspect =
      static_cast<float>(target_size.width()) / target_size.height();
  float actual_aspect = static_cast<float>(column_count) / row_count;
  EXPECT_GE(1.05f, expected_aspect / actual_aspect);
  EXPECT_GE(1.05f, actual_aspect / expected_aspect);
}

TEST_F(ThumbnailContentAnalysisTest, ComputeDecimatedImage) {
  gfx::Size image_size(1600, 1200);
  gfx::Canvas canvas(image_size, 1.0f, true);

  // Make some content we will later want to keep.
  canvas.FillRect(gfx::Rect(100, 200, 100, 100), SkColorSetRGB(125, 0, 0));
  canvas.FillRect(gfx::Rect(300, 200, 100, 100), SkColorSetRGB(0, 200, 0));
  canvas.FillRect(gfx::Rect(500, 200, 100, 100), SkColorSetRGB(0, 0, 225));
  canvas.FillRect(gfx::Rect(100, 400, 600, 100), SkColorSetRGB(125, 200, 225));

  std::vector<bool> rows(image_size.height(), false);
  std::fill_n(rows.begin() + 200, 100, true);
  std::fill_n(rows.begin() + 400, 100, true);

  std::vector<bool> columns(image_size.width(), false);
  std::fill_n(columns.begin() + 100, 100, true);
  std::fill_n(columns.begin() + 300, 100, true);
  std::fill_n(columns.begin() + 500, 100, true);

  SkBitmap source =
      skia::GetTopDevice(*canvas.sk_canvas())->accessBitmap(false);
  SkBitmap result = ComputeDecimatedImage(source, rows, columns);
  EXPECT_FALSE(result.empty());
  EXPECT_EQ(300, result.width());
  EXPECT_EQ(200, result.height());

  // The call should have removed all empty spaces.
  ASSERT_TRUE(CompareImageFragments(source,
                                    result,
                                    gfx::Size(100, 100),
                                    gfx::Point(100, 200),
                                    gfx::Point(0, 0)));
  ASSERT_TRUE(CompareImageFragments(source,
                                    result,
                                    gfx::Size(100, 100),
                                    gfx::Point(300, 200),
                                    gfx::Point(100, 0)));
  ASSERT_TRUE(CompareImageFragments(source,
                                    result,
                                    gfx::Size(100, 100),
                                    gfx::Point(500, 200),
                                    gfx::Point(200, 0)));
  ASSERT_TRUE(CompareImageFragments(source,
                                    result,
                                    gfx::Size(100, 100),
                                    gfx::Point(100, 400),
                                    gfx::Point(0, 100)));
}

TEST_F(ThumbnailContentAnalysisTest, CreateRetargetedThumbnailImage) {
  gfx::Size image_size(1200, 1300);
  gfx::Canvas canvas(image_size, 1.0f, true);

  // The following will create a 'fake image' consisting of color blocks placed
  // on a neutral background. The entire layout is supposed to mimic a
  // screenshot of a web page.
  // The tested function is supposed to locate the interesing areas in the
  // middle.
  const int margin_horizontal = 60;
  const int margin_vertical = 20;
  canvas.FillRect(gfx::Rect(image_size), SkColorSetRGB(200, 210, 210));
  const gfx::Rect header_rect(margin_horizontal,
                              margin_vertical,
                              image_size.width() - 2 * margin_horizontal,
                              100);
  const gfx::Rect footer_rect(margin_horizontal,
                              image_size.height() - margin_vertical - 100,
                              image_size.width() - 2 * margin_horizontal,
                              100);
  const gfx::Rect body_rect(margin_horizontal,
                            header_rect.bottom() + margin_vertical,
                            image_size.width() - 2 * margin_horizontal,
                            footer_rect.y() - header_rect.bottom() -
                            2 * margin_vertical);
  canvas.FillRect(header_rect, SkColorSetRGB(200, 40, 10));
  canvas.FillRect(footer_rect, SkColorSetRGB(10, 40, 180));
  canvas.FillRect(body_rect, SkColorSetRGB(150, 180, 40));

  // 'Fine print' at the bottom.
  const int fine_print = 8;
  const SkColor print_color = SkColorSetRGB(45, 30, 30);
  for (int y = footer_rect.y() + fine_print;
       y < footer_rect.bottom() - fine_print;
       y += 2 * fine_print) {
    for (int x = footer_rect.x() + fine_print;
         x < footer_rect.right() - fine_print;
         x += 2 * fine_print) {
      canvas.DrawRect(gfx::Rect(x, y, fine_print, fine_print),  print_color);
    }
  }

  // Blocky content at the top.
  const int block_size = header_rect.height() - margin_vertical;
  for (int x = header_rect.x() + margin_horizontal;
       x < header_rect.right() - block_size;
       x += block_size + margin_horizontal) {
    const int half_block = block_size / 2 - 5;
    const SkColor block_color = SkColorSetRGB(255, 255, 255);
    const int y = header_rect.y() + margin_vertical / 2;
    int second_col = x + half_block + 10;
    int second_row = y + half_block + 10;
    canvas.FillRect(gfx::Rect(x, y, half_block, block_size), block_color);
    canvas.FillRect(gfx::Rect(second_col,  y, half_block, half_block),
                    block_color);
    canvas.FillRect(gfx::Rect(second_col, second_row, half_block, half_block),
                    block_color);
  }

  // Now the main body. Mostly text with some 'pictures'.
  for (int y = body_rect.y() + fine_print;
       y < body_rect.bottom() - fine_print;
       y += 2 * fine_print) {
    for (int x = body_rect.x() + fine_print;
         x < body_rect.right() - fine_print;
         x += 2 * fine_print) {
      canvas.DrawRect(gfx::Rect(x, y, fine_print, fine_print),  print_color);
    }
  }

  for (int line = 0; line < 3; ++line) {
    int alignment = line % 2;
    const int y = body_rect.y() +
        body_rect.height() / 3 * line + margin_vertical;
    const int x = body_rect.x() +
        alignment * body_rect.width() / 2 + margin_vertical;
    gfx::Rect pict_rect(x, y,
                        body_rect.width() / 2 - 2 * margin_vertical,
                        body_rect.height() / 3 - 2 * margin_vertical);
    canvas.FillRect(pict_rect, SkColorSetRGB(255, 255, 255));
    canvas.DrawRect(pict_rect, SkColorSetRGB(0, 0, 0));
  }

  SkBitmap source =
      skia::GetTopDevice(*canvas.sk_canvas())->accessBitmap(false);

  SkBitmap result = CreateRetargetedThumbnailImage(
      source, gfx::Size(424, 264), 2.5);
  EXPECT_FALSE(result.empty());

  // Given the nature of computation We can't really assert much here about the
  // image itself. We know it should have been computed, should be smaller than
  // the original and it must not be zero.
  EXPECT_LT(result.width(), image_size.width());
  EXPECT_LT(result.height(), image_size.height());

  int histogram[256] = {};
  color_utils::BuildLumaHistogram(result, histogram);
  int non_zero_color_count = std::count_if(
      histogram, histogram + 256, std::bind2nd(std::greater<int>(), 0));
  EXPECT_GT(non_zero_color_count, 4);

}

}  // namespace thumbnailing_utils
