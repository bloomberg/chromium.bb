// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/manifest/manifest_icon_selector.h"

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const int kDefaultIconSize = 144;

GURL FindBestMatchingIconWithMinimum(
    const std::vector<content::Manifest::Icon>& icons,
    int ideal_icon_size_in_px,
    int minimum_icon_size_in_px) {
  return ManifestIconSelector::FindBestMatchingIcon(
      icons, ideal_icon_size_in_px, minimum_icon_size_in_px);
}

GURL FindBestMatchingIcon(const std::vector<content::Manifest::Icon>& icons,
                          int ideal_icon_size_in_px) {
  return FindBestMatchingIconWithMinimum(icons, ideal_icon_size_in_px, 0);
}

static content::Manifest::Icon CreateIcon(const std::string& url,
                                          const std::string& type,
                                          const std::vector<gfx::Size> sizes) {
  content::Manifest::Icon icon;
  icon.src = GURL(url);
  icon.type = base::UTF8ToUTF16(type);
  icon.sizes = sizes;

  return icon;
}

}  // anonymous namespace

TEST(ManifestIconSelector, NoIcons) {
  // No icons should return the empty URL.
  std::vector<content::Manifest::Icon> icons;
  GURL url = FindBestMatchingIcon(icons, kDefaultIconSize);
  EXPECT_TRUE(url.is_empty());
}

TEST(ManifestIconSelector, NoSizes) {
  // Icon with no sizes are ignored.
  std::vector<content::Manifest::Icon> icons;
  icons.push_back(
      CreateIcon("http://foo.com/icon.png", "", std::vector<gfx::Size>()));

  GURL url = FindBestMatchingIcon(icons, kDefaultIconSize);
  EXPECT_TRUE(url.is_empty());
}

TEST(ManifestIconSelector, MIMETypeFiltering) {
  // Icons with type specified to a MIME type that isn't a valid image MIME type
  // are ignored.
  std::vector<gfx::Size> sizes;
  sizes.push_back(gfx::Size(1024, 1024));

  std::vector<content::Manifest::Icon> icons;
  icons.push_back(
      CreateIcon("http://foo.com/icon.png", "image/foo_bar", sizes));
  icons.push_back(CreateIcon("http://foo.com/icon.png", "image/", sizes));
  icons.push_back(CreateIcon("http://foo.com/icon.png", "image/", sizes));
  icons.push_back(CreateIcon("http://foo.com/icon.png", "video/mp4", sizes));

  GURL url = FindBestMatchingIcon(icons, kDefaultIconSize);
  EXPECT_TRUE(url.is_empty());

  icons.clear();
  icons.push_back(CreateIcon("http://foo.com/icon.png", "image/png", sizes));
  url = FindBestMatchingIcon(icons, kDefaultIconSize);
  EXPECT_EQ("http://foo.com/icon.png", url.spec());

  icons.clear();
  icons.push_back(CreateIcon("http://foo.com/icon.png", "image/gif", sizes));
  url = FindBestMatchingIcon(icons, kDefaultIconSize);
  EXPECT_EQ("http://foo.com/icon.png", url.spec());

  icons.clear();
  icons.push_back(CreateIcon("http://foo.com/icon.png", "image/jpeg", sizes));
  url = FindBestMatchingIcon(icons, kDefaultIconSize);
  EXPECT_EQ("http://foo.com/icon.png", url.spec());
}

TEST(ManifestIconSelector, PreferredSizeIsUsedFirst) {
  // Each icon is marked with sizes that match the preferred icon size.
  std::vector<gfx::Size> sizes_48;
  sizes_48.push_back(gfx::Size(48, 48));

  std::vector<gfx::Size> sizes_96;
  sizes_96.push_back(gfx::Size(96, 96));

  std::vector<gfx::Size> sizes_144;
  sizes_144.push_back(gfx::Size(144, 144));

  std::vector<content::Manifest::Icon> icons;
  icons.push_back(CreateIcon("http://foo.com/icon_48.png", "", sizes_48));
  icons.push_back(CreateIcon("http://foo.com/icon_96.png", "", sizes_96));
  icons.push_back(CreateIcon("http://foo.com/icon_144.png", "", sizes_144));

  GURL url = FindBestMatchingIcon(icons, 48);
  EXPECT_EQ("http://foo.com/icon_48.png", url.spec());

  url = FindBestMatchingIcon(icons, 96);
  EXPECT_EQ("http://foo.com/icon_96.png", url.spec());

  url = FindBestMatchingIcon(icons, 144);
  EXPECT_EQ("http://foo.com/icon_144.png", url.spec());
}

TEST(ManifestIconSelector, FirstIconWithPreferredSizeIsUsedFirst) {
  // This test has three icons. The first icon is going to be used because it
  // contains the preferred size.
  std::vector<gfx::Size> sizes_1;
  sizes_1.push_back(gfx::Size(kDefaultIconSize, kDefaultIconSize));
  sizes_1.push_back(gfx::Size(kDefaultIconSize * 2, kDefaultIconSize * 2));
  sizes_1.push_back(gfx::Size(kDefaultIconSize * 3, kDefaultIconSize * 3));

  std::vector<gfx::Size> sizes_2;
  sizes_2.push_back(gfx::Size(1024, 1024));

  std::vector<gfx::Size> sizes_3;
  sizes_3.push_back(gfx::Size(1024, 1024));

  std::vector<content::Manifest::Icon> icons;
  icons.push_back(CreateIcon("http://foo.com/icon_x1.png", "", sizes_1));
  icons.push_back(CreateIcon("http://foo.com/icon_x2.png", "", sizes_2));
  icons.push_back(CreateIcon("http://foo.com/icon_x3.png", "", sizes_3));

  GURL url = FindBestMatchingIcon(icons, kDefaultIconSize);
  EXPECT_EQ("http://foo.com/icon_x1.png", url.spec());

  url = FindBestMatchingIcon(icons, kDefaultIconSize * 2);
  EXPECT_EQ("http://foo.com/icon_x1.png", url.spec());

  url = FindBestMatchingIcon(icons, kDefaultIconSize * 3);
  EXPECT_EQ("http://foo.com/icon_x1.png", url.spec());
}

TEST(ManifestIconSelector, FallbackToSmallestLargerIcon) {
  // If there is no perfect icon, the smallest larger icon will be chosen.
  std::vector<gfx::Size> sizes_1;
  sizes_1.push_back(gfx::Size(90, 90));

  std::vector<gfx::Size> sizes_2;
  sizes_2.push_back(gfx::Size(128, 128));

  std::vector<gfx::Size> sizes_3;
  sizes_3.push_back(gfx::Size(192, 192));

  std::vector<content::Manifest::Icon> icons;
  icons.push_back(CreateIcon("http://foo.com/icon_x1.png", "", sizes_1));
  icons.push_back(CreateIcon("http://foo.com/icon_x2.png", "", sizes_2));
  icons.push_back(CreateIcon("http://foo.com/icon_x3.png", "", sizes_3));

  GURL url = FindBestMatchingIcon(icons, 48);
  EXPECT_EQ("http://foo.com/icon_x1.png", url.spec());

  url = FindBestMatchingIcon(icons, 96);
  EXPECT_EQ("http://foo.com/icon_x2.png", url.spec());

  url = FindBestMatchingIcon(icons, 144);
  EXPECT_EQ("http://foo.com/icon_x3.png", url.spec());
}

TEST(ManifestIconSelector, FallbackToLargestIconLargerThanMinimum) {
  // When an icon of the correct size has not been found, we fall back to the
  // closest non-matching sizes. Make sure that the minimum passed is enforced.
  std::vector<gfx::Size> sizes_1_2;
  std::vector<gfx::Size> sizes_3;

  sizes_1_2.push_back(gfx::Size(47, 47));
  sizes_3.push_back(gfx::Size(95, 95));

  std::vector<content::Manifest::Icon> icons;
  icons.push_back(CreateIcon("http://foo.com/icon_x1.png", "", sizes_1_2));
  icons.push_back(CreateIcon("http://foo.com/icon_x2.png", "", sizes_1_2));
  icons.push_back(CreateIcon("http://foo.com/icon_x3.png", "", sizes_3));

  // Icon 3 should match.
  GURL url = FindBestMatchingIconWithMinimum(icons, 1024, 48);
  EXPECT_EQ("http://foo.com/icon_x3.png", url.spec());

  // Nothing matches here as the minimum is 96.
  url = FindBestMatchingIconWithMinimum(icons, 1024, 96);
  EXPECT_TRUE(url.is_empty());
}

TEST(ManifestIconSelector, IdealVeryCloseToMinimumMatches) {
  std::vector<gfx::Size> sizes;
  sizes.push_back(gfx::Size(2, 2));

  std::vector<content::Manifest::Icon> icons;
  icons.push_back(CreateIcon("http://foo.com/icon_x1.png", "", sizes));

  GURL url = FindBestMatchingIconWithMinimum(icons, 2, 1);
  EXPECT_EQ("http://foo.com/icon_x1.png", url.spec());
}

TEST(ManifestIconSelector, SizeVeryCloseToMinimumMatches) {
  std::vector<gfx::Size> sizes;
  sizes.push_back(gfx::Size(2, 2));

  std::vector<content::Manifest::Icon> icons;
  icons.push_back(CreateIcon("http://foo.com/icon_x1.png", "", sizes));

  GURL url = FindBestMatchingIconWithMinimum(icons, 200, 1);
  EXPECT_EQ("http://foo.com/icon_x1.png", url.spec());
}

TEST(ManifestIconSelector, NotSquareIconsAreIgnored) {
  std::vector<gfx::Size> sizes;
  sizes.push_back(gfx::Size(1024, 1023));

  std::vector<content::Manifest::Icon> icons;
  icons.push_back(CreateIcon("http://foo.com/icon.png", "", sizes));

  GURL url = FindBestMatchingIcon(icons, kDefaultIconSize);
  EXPECT_TRUE(url.is_empty());
}

TEST(ManifestIconSelector, ClosestIconToPreferred) {
  // Ensure ManifestIconSelector::FindBestMatchingIcon selects the closest icon
  // to the preferred size when presented with a number of options.
  int very_small = kDefaultIconSize / 4;
  int small_size = kDefaultIconSize / 2;
  int bit_small = kDefaultIconSize - 1;
  int bit_big = kDefaultIconSize + 1;
  int big = kDefaultIconSize * 2;
  int very_big = kDefaultIconSize * 4;

  // (very_small, bit_small) => bit_small
  {
    std::vector<gfx::Size> sizes_1;
    sizes_1.push_back(gfx::Size(very_small, very_small));

    std::vector<gfx::Size> sizes_2;
    sizes_2.push_back(gfx::Size(bit_small, bit_small));

    std::vector<content::Manifest::Icon> icons;
    icons.push_back(CreateIcon("http://foo.com/icon_no.png", "", sizes_1));
    icons.push_back(CreateIcon("http://foo.com/icon.png", "", sizes_2));

    GURL url = FindBestMatchingIcon(icons, kDefaultIconSize);
    EXPECT_EQ("http://foo.com/icon.png", url.spec());
  }

  // (very_small, bit_small, small_size) => bit_small
  {
    std::vector<gfx::Size> sizes_1;
    sizes_1.push_back(gfx::Size(very_small, very_small));

    std::vector<gfx::Size> sizes_2;
    sizes_2.push_back(gfx::Size(bit_small, bit_small));

    std::vector<gfx::Size> sizes_3;
    sizes_3.push_back(gfx::Size(small_size, small_size));

    std::vector<content::Manifest::Icon> icons;
    icons.push_back(CreateIcon("http://foo.com/icon_no_1.png", "", sizes_1));
    icons.push_back(CreateIcon("http://foo.com/icon.png", "", sizes_2));
    icons.push_back(CreateIcon("http://foo.com/icon_no_2.png", "", sizes_3));

    GURL url = FindBestMatchingIcon(icons, kDefaultIconSize);
    EXPECT_EQ("http://foo.com/icon.png", url.spec());
  }

  // (very_big, big) => big
  {
    std::vector<gfx::Size> sizes_1;
    sizes_1.push_back(gfx::Size(very_big, very_big));

    std::vector<gfx::Size> sizes_2;
    sizes_2.push_back(gfx::Size(big, big));

    std::vector<content::Manifest::Icon> icons;
    icons.push_back(CreateIcon("http://foo.com/icon_no.png", "", sizes_1));
    icons.push_back(CreateIcon("http://foo.com/icon.png", "", sizes_2));

    GURL url = FindBestMatchingIcon(icons, kDefaultIconSize);
    EXPECT_EQ("http://foo.com/icon.png", url.spec());
  }

  // (very_big, big, bit_big) => bit_big
  {
    std::vector<gfx::Size> sizes_1;
    sizes_1.push_back(gfx::Size(very_big, very_big));

    std::vector<gfx::Size> sizes_2;
    sizes_2.push_back(gfx::Size(big, big));

    std::vector<gfx::Size> sizes_3;
    sizes_3.push_back(gfx::Size(bit_big, bit_big));

    std::vector<content::Manifest::Icon> icons;
    icons.push_back(CreateIcon("http://foo.com/icon_no.png", "", sizes_1));
    icons.push_back(CreateIcon("http://foo.com/icon_no.png", "", sizes_2));
    icons.push_back(CreateIcon("http://foo.com/icon.png", "", sizes_3));

    GURL url = FindBestMatchingIcon(icons, kDefaultIconSize);
    EXPECT_EQ("http://foo.com/icon.png", url.spec());
  }

  // (bit_small, very_big) => very_big
  {
    std::vector<gfx::Size> sizes_1;
    sizes_1.push_back(gfx::Size(bit_small, bit_small));

    std::vector<gfx::Size> sizes_2;
    sizes_2.push_back(gfx::Size(very_big, very_big));

    std::vector<content::Manifest::Icon> icons;
    icons.push_back(CreateIcon("http://foo.com/icon_no.png", "", sizes_1));
    icons.push_back(CreateIcon("http://foo.com/icon.png", "", sizes_2));

    GURL url = FindBestMatchingIcon(icons, kDefaultIconSize);
    EXPECT_EQ("http://foo.com/icon.png", url.spec());
  }

  // (bit_small, bit_big) => bit_big
  {
    std::vector<gfx::Size> sizes_1;
    sizes_1.push_back(gfx::Size(bit_small, bit_small));

    std::vector<gfx::Size> sizes_2;
    sizes_2.push_back(gfx::Size(bit_big, bit_big));

    std::vector<content::Manifest::Icon> icons;
    icons.push_back(CreateIcon("http://foo.com/icon_no.png", "", sizes_1));
    icons.push_back(CreateIcon("http://foo.com/icon.png", "", sizes_2));

    GURL url = FindBestMatchingIcon(icons, kDefaultIconSize);
    EXPECT_EQ("http://foo.com/icon.png", url.spec());
  }
}

TEST(ManifestIconSelector, UseAnyIfNoPreferredSize) {
  // 'any' (ie. gfx::Size(0,0)) should be used if there is no icon of a
  // preferred size.

  // Icon with 'any' and icon with preferred size => preferred size is chosen.
  {
    std::vector<gfx::Size> sizes_1;
    sizes_1.push_back(gfx::Size(kDefaultIconSize, kDefaultIconSize));
    std::vector<gfx::Size> sizes_2;
    sizes_2.push_back(gfx::Size(0, 0));

    std::vector<content::Manifest::Icon> icons;
    icons.push_back(CreateIcon("http://foo.com/icon.png", "", sizes_1));
    icons.push_back(CreateIcon("http://foo.com/icon_no.png", "", sizes_2));

    GURL url = FindBestMatchingIcon(icons, kDefaultIconSize);
    EXPECT_EQ("http://foo.com/icon.png", url.spec());
  }

  // Icon with 'any' and icon larger than preferred size => any is chosen.
  {
    std::vector<gfx::Size> sizes_1;
    sizes_1.push_back(gfx::Size(kDefaultIconSize + 1, kDefaultIconSize + 1));
    std::vector<gfx::Size> sizes_2;
    sizes_2.push_back(gfx::Size(0, 0));

    std::vector<content::Manifest::Icon> icons;
    icons.push_back(CreateIcon("http://foo.com/icon_no.png", "", sizes_1));
    icons.push_back(CreateIcon("http://foo.com/icon.png", "", sizes_2));

    GURL url = FindBestMatchingIcon(icons, kDefaultIconSize);
    EXPECT_EQ("http://foo.com/icon.png", url.spec());
  }

  // Multiple icons with 'any' => the last one is chosen.
  {
    std::vector<gfx::Size> sizes;
    sizes.push_back(gfx::Size(0, 0));

    std::vector<content::Manifest::Icon> icons;
    icons.push_back(CreateIcon("http://foo.com/icon_no1.png", "", sizes));
    icons.push_back(CreateIcon("http://foo.com/icon_no2.png", "", sizes));
    icons.push_back(CreateIcon("http://foo.com/icon.png", "", sizes));

    GURL url = FindBestMatchingIcon(icons, kDefaultIconSize * 3);
    EXPECT_EQ("http://foo.com/icon.png", url.spec());
  }
}
