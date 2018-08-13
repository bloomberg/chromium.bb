// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_icon_generator.h"

#include <vector>

#include "base/stl_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace web_app {

namespace {

const char kAppIconURL1[] = "http://foo.com/1.png";
const char kAppIconURL2[] = "http://foo.com/2.png";
const char kAppIconURL3[] = "http://foo.com/3.png";

// These sizes match extension_misc::ExtensionIcons enum.
const int kIconSizeTiny = 16;
const int kIconSizeSmall = 32;
const int kIconSizeMedium = 48;
const int kIconSizeLarge = 128;
const int kIconSizeGigantor = 512;

const int kIconSizeSmallBetweenMediumAndLarge = 63;
const int kIconSizeLargeBetweenMediumAndLarge = 96;

BitmapAndSource CreateSquareIcon(const GURL& gurl, int size, SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(size, size);
  bitmap.eraseColor(color);
  return BitmapAndSource(gurl, bitmap);
}

std::set<int> TestSizesToGenerate() {
  const int kIconSizesToGenerate[] = {
      kIconSizeSmall, kIconSizeMedium, kIconSizeLarge,
  };
  return std::set<int>(kIconSizesToGenerate,
                       kIconSizesToGenerate + base::size(kIconSizesToGenerate));
}

void ValidateAllIconsWithURLsArePresent(
    const std::vector<BitmapAndSource>& icons_to_check,
    const std::map<int, BitmapAndSource>& size_map) {
  EXPECT_EQ(icons_to_check.size(), size_map.size());

  // Check that every icon with URL has a mapped icon.
  for (const auto& icon : icons_to_check) {
    if (!icon.source_url.is_empty()) {
      bool found = false;
      if (base::ContainsKey(size_map, icon.bitmap.width())) {
        const BitmapAndSource& mapped_icon = size_map.at(icon.bitmap.width());
        if (mapped_icon.source_url == icon.source_url &&
            mapped_icon.bitmap.width() == icon.bitmap.width()) {
          found = true;
        }
      }
      EXPECT_TRUE(found);
    }
  }
}

std::vector<BitmapAndSource>::const_iterator FindLargestBitmapAndSourceVector(
    const std::vector<BitmapAndSource>& bitmap_vector) {
  auto result = bitmap_vector.end();
  int largest = -1;
  for (auto it = bitmap_vector.begin(); it != bitmap_vector.end(); ++it) {
    if (it->bitmap.width() > largest) {
      result = it;
    }
  }
  return result;
}

std::vector<BitmapAndSource>::const_iterator FindMatchingBitmapAndSourceVector(
    const std::vector<BitmapAndSource>& bitmap_vector,
    int size) {
  for (auto it = bitmap_vector.begin(); it != bitmap_vector.end(); ++it) {
    if (it->bitmap.width() == size) {
      return it;
    }
  }
  return bitmap_vector.end();
}

std::vector<BitmapAndSource>::const_iterator
FindEqualOrLargerBitmapAndSourceVector(
    const std::vector<BitmapAndSource>& bitmap_vector,
    int size) {
  for (auto it = bitmap_vector.begin(); it != bitmap_vector.end(); ++it) {
    if (it->bitmap.width() >= size) {
      return it;
    }
  }
  return bitmap_vector.end();
}

void ValidateIconsGeneratedAndResizedCorrectly(
    std::vector<BitmapAndSource> downloaded,
    std::map<int, BitmapAndSource> size_map,
    std::set<int> sizes_to_generate,
    int expected_generated,
    int expected_resized) {
  GURL empty_url("");
  int number_generated = 0;
  int number_resized = 0;

  auto icon_largest = FindLargestBitmapAndSourceVector(downloaded);
  for (const auto& size : sizes_to_generate) {
    auto icon_downloaded = FindMatchingBitmapAndSourceVector(downloaded, size);
    auto icon_larger = FindEqualOrLargerBitmapAndSourceVector(downloaded, size);
    if (icon_downloaded == downloaded.end()) {
      auto icon_resized = size_map.find(size);
      if (icon_largest == downloaded.end()) {
        // There are no downloaded icons. Expect an icon to be generated.
        EXPECT_NE(size_map.end(), icon_resized);
        EXPECT_EQ(size, icon_resized->second.bitmap.width());
        EXPECT_EQ(size, icon_resized->second.bitmap.height());
        EXPECT_EQ(size, icon_resized->second.bitmap.height());
        EXPECT_EQ(empty_url, icon_resized->second.source_url);
        ++number_generated;
      } else {
        // If there is a larger downloaded icon, it should be resized. Otherwise
        // the largest downloaded icon should be resized.
        auto icon_to_resize = icon_largest;
        if (icon_larger != downloaded.end())
          icon_to_resize = icon_larger;
        EXPECT_NE(size_map.end(), icon_resized);
        EXPECT_EQ(size, icon_resized->second.bitmap.width());
        EXPECT_EQ(size, icon_resized->second.bitmap.height());
        EXPECT_EQ(size, icon_resized->second.bitmap.height());
        EXPECT_EQ(icon_to_resize->source_url, icon_resized->second.source_url);
        ++number_resized;
      }
    } else {
      // There is an icon of exactly this size downloaded. Expect no icon to be
      // generated, and the existing downloaded icon to be used.
      auto icon_resized = size_map.find(size);
      EXPECT_NE(size_map.end(), icon_resized);
      EXPECT_EQ(size, icon_resized->second.bitmap.width());
      EXPECT_EQ(size, icon_resized->second.bitmap.height());
      EXPECT_EQ(size, icon_downloaded->bitmap.width());
      EXPECT_EQ(size, icon_downloaded->bitmap.height());
      EXPECT_EQ(icon_downloaded->source_url, icon_resized->second.source_url);
    }
  }
  EXPECT_EQ(expected_generated, number_generated);
  EXPECT_EQ(expected_resized, number_resized);
}

void ValidateBitmapSizeAndColor(SkBitmap bitmap, int size, SkColor color) {
  // Obtain pixel lock to access pixels.
  EXPECT_EQ(color, bitmap.getColor(0, 0));
  EXPECT_EQ(size, bitmap.width());
  EXPECT_EQ(size, bitmap.height());
}

void TestIconGeneration(int icon_size,
                        int expected_generated,
                        int expected_resized) {
  std::vector<BitmapAndSource> downloaded;

  // Add an icon with a URL and bitmap. 'Download' it.
  downloaded.push_back(
      CreateSquareIcon(GURL(kAppIconURL1), icon_size, SK_ColorRED));

  // Now run the resizing/generation and validation.
  SkColor generated_icon_color = SK_ColorTRANSPARENT;
  auto size_map = ResizeIconsAndGenerateMissing(
      downloaded, TestSizesToGenerate(), GURL(), &generated_icon_color);

  ValidateIconsGeneratedAndResizedCorrectly(
      downloaded, size_map, TestSizesToGenerate(), expected_generated,
      expected_resized);
}

}  // namespace

TEST(WebAppIconGeneratorTest, ConstrainBitmapsToSizes) {
  std::set<int> desired_sizes;
  desired_sizes.insert(16);
  desired_sizes.insert(32);
  desired_sizes.insert(48);
  desired_sizes.insert(96);
  desired_sizes.insert(128);
  desired_sizes.insert(256);

  {
    std::vector<BitmapAndSource> bitmaps;
    bitmaps.push_back(CreateSquareIcon(GURL(), 16, SK_ColorRED));
    bitmaps.push_back(CreateSquareIcon(GURL(), 32, SK_ColorGREEN));
    bitmaps.push_back(CreateSquareIcon(GURL(), 144, SK_ColorYELLOW));

    std::map<int, BitmapAndSource> results =
        ConstrainBitmapsToSizes(bitmaps, desired_sizes);

    EXPECT_EQ(6u, results.size());
    ValidateBitmapSizeAndColor(results[16].bitmap, 16, SK_ColorRED);
    ValidateBitmapSizeAndColor(results[32].bitmap, 32, SK_ColorGREEN);
    ValidateBitmapSizeAndColor(results[48].bitmap, 48, SK_ColorYELLOW);
    ValidateBitmapSizeAndColor(results[96].bitmap, 96, SK_ColorYELLOW);
    ValidateBitmapSizeAndColor(results[128].bitmap, 128, SK_ColorYELLOW);
    ValidateBitmapSizeAndColor(results[256].bitmap, 256, SK_ColorYELLOW);
  }
  {
    std::vector<BitmapAndSource> bitmaps;
    bitmaps.push_back(CreateSquareIcon(GURL(), 512, SK_ColorRED));
    bitmaps.push_back(CreateSquareIcon(GURL(), 18, SK_ColorGREEN));
    bitmaps.push_back(CreateSquareIcon(GURL(), 33, SK_ColorBLUE));
    bitmaps.push_back(CreateSquareIcon(GURL(), 17, SK_ColorYELLOW));

    std::map<int, BitmapAndSource> results =
        ConstrainBitmapsToSizes(bitmaps, desired_sizes);

    EXPECT_EQ(6u, results.size());
    ValidateBitmapSizeAndColor(results[16].bitmap, 16, SK_ColorYELLOW);
    ValidateBitmapSizeAndColor(results[32].bitmap, 32, SK_ColorBLUE);
    ValidateBitmapSizeAndColor(results[48].bitmap, 48, SK_ColorRED);
    ValidateBitmapSizeAndColor(results[96].bitmap, 96, SK_ColorRED);
    ValidateBitmapSizeAndColor(results[128].bitmap, 128, SK_ColorRED);
    ValidateBitmapSizeAndColor(results[256].bitmap, 256, SK_ColorRED);
  }
}

TEST(WebAppIconGeneratorTest, LinkedAppIconsAreNotChanged) {
  std::vector<BitmapAndSource> icons;

  const GURL url = GURL(kAppIconURL3);
  const SkColor color = SK_ColorBLACK;

  icons.push_back(CreateSquareIcon(url, kIconSizeMedium, color));
  icons.push_back(CreateSquareIcon(url, kIconSizeSmall, color));
  icons.push_back(CreateSquareIcon(url, kIconSizeLarge, color));

  // 'Download' one of the icons without a size or bitmap.
  std::vector<BitmapAndSource> downloaded;
  downloaded.push_back(CreateSquareIcon(url, kIconSizeLarge, color));

  const auto& sizes = TestSizesToGenerate();

  // Now run the resizing and generation into a new web icons info.
  SkColor generated_icon_color = SK_ColorTRANSPARENT;
  std::map<int, BitmapAndSource> size_map = ResizeIconsAndGenerateMissing(
      downloaded, sizes, GURL(), &generated_icon_color);
  EXPECT_EQ(sizes.size(), size_map.size());

  // Now check that the linked app icons (i.e. those with URLs) are matching.
  ValidateAllIconsWithURLsArePresent(icons, size_map);
}

TEST(WebAppIconGeneratorTest, IconsResizedFromOddSizes) {
  std::vector<BitmapAndSource> downloaded;

  const SkColor color = SK_ColorRED;

  // Add three icons with a URL and bitmap. 'Download' each of them.
  downloaded.push_back(
      CreateSquareIcon(GURL(kAppIconURL1), kIconSizeSmall, color));
  downloaded.push_back(CreateSquareIcon(
      GURL(kAppIconURL2), kIconSizeSmallBetweenMediumAndLarge, color));
  downloaded.push_back(CreateSquareIcon(
      GURL(kAppIconURL3), kIconSizeLargeBetweenMediumAndLarge, color));

  // Now run the resizing and generation.
  SkColor generated_icon_color = SK_ColorTRANSPARENT;
  std::map<int, BitmapAndSource> size_map = ResizeIconsAndGenerateMissing(
      downloaded, TestSizesToGenerate(), GURL(), &generated_icon_color);

  // No icons should be generated. The LARGE and MEDIUM sizes should be resized.
  ValidateIconsGeneratedAndResizedCorrectly(downloaded, size_map,
                                            TestSizesToGenerate(), 0, 2);
}

TEST(WebAppIconGeneratorTest, IconsResizedFromLarger) {
  std::vector<BitmapAndSource> downloaded;

  // Add three icons with a URL and bitmap. 'Download' two of them and pretend
  // the third failed to download.
  downloaded.push_back(
      CreateSquareIcon(GURL(kAppIconURL1), kIconSizeSmall, SK_ColorRED));
  downloaded.push_back(
      CreateSquareIcon(GURL(kAppIconURL3), kIconSizeGigantor, SK_ColorBLACK));

  // Now run the resizing and generation.
  SkColor generated_icon_color = SK_ColorTRANSPARENT;
  std::map<int, BitmapAndSource> size_map = ResizeIconsAndGenerateMissing(
      downloaded, TestSizesToGenerate(), GURL(), &generated_icon_color);

  // Expect icon for MEDIUM and LARGE to be resized from the gigantor icon
  // as it was not downloaded.
  ValidateIconsGeneratedAndResizedCorrectly(downloaded, size_map,
                                            TestSizesToGenerate(), 0, 2);
}

TEST(WebAppIconGeneratorTest, AllIconsGeneratedWhenNotDownloaded) {
  // Add three icons with a URL and bitmap. 'Download' none of them.
  std::vector<BitmapAndSource> downloaded;

  // Now run the resizing and generation.
  SkColor generated_icon_color = SK_ColorTRANSPARENT;
  std::map<int, BitmapAndSource> size_map = ResizeIconsAndGenerateMissing(
      downloaded, TestSizesToGenerate(), GURL(), &generated_icon_color);

  // Expect all icons to be generated.
  ValidateIconsGeneratedAndResizedCorrectly(downloaded, size_map,
                                            TestSizesToGenerate(), 3, 0);
}

TEST(WebAppIconGeneratorTest, IconResizedFromLargerAndSmaller) {
  std::vector<BitmapAndSource> downloaded;

  // Pretend the huge icon wasn't downloaded but two smaller ones were.
  downloaded.push_back(
      CreateSquareIcon(GURL(kAppIconURL1), kIconSizeTiny, SK_ColorRED));
  downloaded.push_back(
      CreateSquareIcon(GURL(kAppIconURL2), kIconSizeMedium, SK_ColorBLUE));

  // Now run the resizing and generation.
  SkColor generated_icon_color = SK_ColorTRANSPARENT;
  std::map<int, BitmapAndSource> size_map = ResizeIconsAndGenerateMissing(
      downloaded, TestSizesToGenerate(), GURL(), &generated_icon_color);

  // Expect no icons to be generated, but the LARGE and SMALL icons to be
  // resized from the MEDIUM icon.
  ValidateIconsGeneratedAndResizedCorrectly(downloaded, size_map,
                                            TestSizesToGenerate(), 0, 2);

  // Verify specifically that the LARGE icons was resized from the medium icon.
  const auto it = size_map.find(kIconSizeLarge);
  EXPECT_NE(size_map.end(), it);
  EXPECT_EQ(GURL(kAppIconURL2), it->second.source_url);
}

TEST(WebAppIconGeneratorTest, IconsResizedWhenOnlyATinyOneIsProvided) {
  // When only a tiny icon is downloaded (smaller than the three desired
  // sizes), 3 icons should be resized.
  TestIconGeneration(kIconSizeTiny, 0, 3);
}

TEST(WebAppIconGeneratorTest, IconsResizedWhenOnlyAGigantorOneIsProvided) {
  // When an enormous icon is provided, each desired icon size should be resized
  // from it, and no icons should be generated.
  TestIconGeneration(kIconSizeGigantor, 0, 3);
}

}  // namespace web_app
