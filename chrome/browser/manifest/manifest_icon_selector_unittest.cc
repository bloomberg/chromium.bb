// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/manifest/manifest_icon_selector.h"

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/screen.h"
#include "ui/display/test/test_screen.h"

namespace {
const int DEFAULT_PREFERRED_ICON_SIZE = 48;
}

class ManifestIconSelectorTest : public testing::Test  {
 protected:
  ManifestIconSelectorTest() {
    test_screen_.display()->set_id(0x1337);
    test_screen_.display()->set_bounds(gfx::Rect(0, 0, 2560, 1440));
    display::Screen::SetScreenInstance(&test_screen_);
  }

  ~ManifestIconSelectorTest() override {
    display::Screen::SetScreenInstance(nullptr);
  }

  void SetUp() override {
    SetPreferredIconSizeInDp(DEFAULT_PREFERRED_ICON_SIZE);
  }

  GURL FindBestMatchingIconWithMinimum(
      const std::vector<content::Manifest::Icon>& icons,
      int minimum_icon_size_in_dp) {
    return ManifestIconSelector::FindBestMatchingIcon(
        icons, GetPreferredIconSizeInDp(), minimum_icon_size_in_dp);
  }

  GURL FindBestMatchingIcon(const std::vector<content::Manifest::Icon>& icons) {
    return FindBestMatchingIconWithMinimum(icons, 0);
  }

  void SetDisplayDeviceScaleFactor(float device_scale_factor) {
    test_screen_.display()->set_device_scale_factor(device_scale_factor);
  }

  int GetPreferredIconSizeInDp() {
    return preferred_icon_size_;
  }

  void SetPreferredIconSizeInDp(int new_size) {
    preferred_icon_size_ = new_size;
  }

  static content::Manifest::Icon CreateIcon(
      const std::string& url,
      const std::string& type,
      const std::vector<gfx::Size> sizes) {
    content::Manifest::Icon icon;
    icon.src = GURL(url);
    if (!type.empty())
      icon.type = base::NullableString16(base::UTF8ToUTF16(type), false);
    icon.sizes = sizes;

    return icon;
  }

 private:
  display::test::TestScreen test_screen_;
  int preferred_icon_size_ = DEFAULT_PREFERRED_ICON_SIZE;

  DISALLOW_COPY_AND_ASSIGN(ManifestIconSelectorTest);
};

TEST_F(ManifestIconSelectorTest, NoIcons) {
  // No icons should return the empty URL.
  std::vector<content::Manifest::Icon> icons;
  GURL url = FindBestMatchingIcon(icons);
  EXPECT_TRUE(url.is_empty());
}

TEST_F(ManifestIconSelectorTest, NoSizes) {
  // Icon with no sizes are ignored.
  std::vector<content::Manifest::Icon> icons;
  icons.push_back(
      CreateIcon("http://foo.com/icon.png", "", std::vector<gfx::Size>()));

  GURL url = FindBestMatchingIcon(icons);
  EXPECT_TRUE(url.is_empty());
}

TEST_F(ManifestIconSelectorTest, MIMETypeFiltering) {
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

  GURL url = FindBestMatchingIcon(icons);
  EXPECT_TRUE(url.is_empty());

  icons.clear();
  icons.push_back(CreateIcon("http://foo.com/icon.png", "image/png", sizes));
  url = FindBestMatchingIcon(icons);
  EXPECT_EQ("http://foo.com/icon.png", url.spec());

  icons.clear();
  icons.push_back(CreateIcon("http://foo.com/icon.png", "image/gif", sizes));
  url = FindBestMatchingIcon(icons);
  EXPECT_EQ("http://foo.com/icon.png", url.spec());

  icons.clear();
  icons.push_back(CreateIcon("http://foo.com/icon.png", "image/jpeg", sizes));
  url = FindBestMatchingIcon(icons);
  EXPECT_EQ("http://foo.com/icon.png", url.spec());
}

TEST_F(ManifestIconSelectorTest, PreferredSizeIsUsedFirst) {
  // Each icon is marked with sizes that match the preferred icon size.
  std::vector<gfx::Size> sizes_1;
  sizes_1.push_back(gfx::Size(GetPreferredIconSizeInDp(),
                              GetPreferredIconSizeInDp()));

  std::vector<gfx::Size> sizes_2;
  sizes_2.push_back(gfx::Size(GetPreferredIconSizeInDp() * 2,
                              GetPreferredIconSizeInDp() * 2));

  std::vector<gfx::Size> sizes_3;
  sizes_3.push_back(gfx::Size(GetPreferredIconSizeInDp() * 3,
                              GetPreferredIconSizeInDp() * 3));

  std::vector<content::Manifest::Icon> icons;
  icons.push_back(CreateIcon("http://foo.com/icon_x1.png", "", sizes_1));
  icons.push_back(CreateIcon("http://foo.com/icon_x2.png", "", sizes_2));
  icons.push_back(CreateIcon("http://foo.com/icon_x3.png", "", sizes_3));

  SetDisplayDeviceScaleFactor(1.0f);
  GURL url = FindBestMatchingIcon(icons);
  EXPECT_EQ("http://foo.com/icon_x1.png", url.spec());

  SetDisplayDeviceScaleFactor(2.0f);
  url = FindBestMatchingIcon(icons);
  EXPECT_EQ("http://foo.com/icon_x2.png", url.spec());

  SetDisplayDeviceScaleFactor(3.0f);
  url = FindBestMatchingIcon(icons);
  EXPECT_EQ("http://foo.com/icon_x3.png", url.spec());
}

TEST_F(ManifestIconSelectorTest, FirstIconWithPreferredSizeIsUsedFirst) {
  // This test has three icons. The first icon is going to be used because it
  // has sizes which matches the preferred size for each device scale factor.
  std::vector<gfx::Size> sizes_1;
  sizes_1.push_back(gfx::Size(GetPreferredIconSizeInDp(),
                              GetPreferredIconSizeInDp()));
  sizes_1.push_back(gfx::Size(GetPreferredIconSizeInDp() * 2,
                              GetPreferredIconSizeInDp() * 2));
  sizes_1.push_back(gfx::Size(GetPreferredIconSizeInDp() * 3,
                              GetPreferredIconSizeInDp() * 3));

  std::vector<gfx::Size> sizes_2;
  sizes_2.push_back(gfx::Size(1024, 1024));

  std::vector<gfx::Size> sizes_3;
  sizes_3.push_back(gfx::Size(1024, 1024));

  std::vector<content::Manifest::Icon> icons;
  icons.push_back(CreateIcon("http://foo.com/icon_x1.png", "", sizes_1));
  icons.push_back(CreateIcon("http://foo.com/icon_x2.png", "", sizes_2));
  icons.push_back(CreateIcon("http://foo.com/icon_x3.png", "", sizes_3));

  SetDisplayDeviceScaleFactor(1.0f);
  GURL url = FindBestMatchingIcon(icons);
  EXPECT_EQ("http://foo.com/icon_x1.png", url.spec());

  SetDisplayDeviceScaleFactor(2.0f);
  url = FindBestMatchingIcon(icons);
  EXPECT_EQ("http://foo.com/icon_x1.png", url.spec());

  SetDisplayDeviceScaleFactor(3.0f);
  url = FindBestMatchingIcon(icons);
  EXPECT_EQ("http://foo.com/icon_x1.png", url.spec());
}

TEST_F(ManifestIconSelectorTest, UseDeviceDensity) {
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

  SetDisplayDeviceScaleFactor(1.0f);
  GURL url = FindBestMatchingIcon(icons);
  EXPECT_EQ("http://foo.com/icon_x1.png", url.spec());

  SetDisplayDeviceScaleFactor(2.0f);
  url = FindBestMatchingIcon(icons);
  EXPECT_EQ("http://foo.com/icon_x2.png", url.spec());

  SetDisplayDeviceScaleFactor(3.0f);
  url = FindBestMatchingIcon(icons);
  EXPECT_EQ("http://foo.com/icon_x3.png", url.spec());
}

TEST_F(ManifestIconSelectorTest, CheckDifferentDeviceScaleFactors) {
  // When an icon of the correct size has not been found, we fall back to the
  // closest non-matching sizes. Make sure that the minimum passed is enforced.
  std::vector<gfx::Size> sizes_1_2;
  std::vector<gfx::Size> sizes_3;

  sizes_1_2.push_back(gfx::Size(47, 47));
  sizes_3.push_back(gfx::Size(95, 95));

  // Set to a value which will not affect the calculations.
  SetPreferredIconSizeInDp(1024);

  std::vector<content::Manifest::Icon> icons;
  icons.push_back(CreateIcon("http://foo.com/icon_x1.png", "", sizes_1_2));
  icons.push_back(CreateIcon("http://foo.com/icon_x2.png", "", sizes_1_2));
  icons.push_back(CreateIcon("http://foo.com/icon_x3.png", "", sizes_3));

  // Icon 3 should match.
  SetDisplayDeviceScaleFactor(1.0f);
  GURL url = FindBestMatchingIconWithMinimum(icons, 48);
  EXPECT_EQ("http://foo.com/icon_x3.png", url.spec());

  // Nothing matches here because the minimum is 48 and all icon sizes are now
  // too small.
  SetDisplayDeviceScaleFactor(2.0f);
  url = FindBestMatchingIconWithMinimum(icons, 48);
  EXPECT_TRUE(url.is_empty());

  // Nothing matches here as the minimum is 96.
  SetDisplayDeviceScaleFactor(3.0f);
  url = FindBestMatchingIconWithMinimum(icons, 96);
  EXPECT_TRUE(url.is_empty());
}

TEST_F(ManifestIconSelectorTest, IdealVeryCloseToMinimumMatches) {
  std::vector<gfx::Size> sizes;
  sizes.push_back(gfx::Size(2, 2));
  SetPreferredIconSizeInDp(2);

  std::vector<content::Manifest::Icon> icons;
  icons.push_back(CreateIcon("http://foo.com/icon_x1.png", "", sizes));

  SetDisplayDeviceScaleFactor(1.0f);
  GURL url = FindBestMatchingIconWithMinimum(icons, 1);
  EXPECT_EQ("http://foo.com/icon_x1.png", url.spec());
}

TEST_F(ManifestIconSelectorTest, SizeVeryCloseToMinimumMatches) {
  std::vector<gfx::Size> sizes;
  sizes.push_back(gfx::Size(2, 2));

  SetPreferredIconSizeInDp(200);

  std::vector<content::Manifest::Icon> icons;
  icons.push_back(CreateIcon("http://foo.com/icon_x1.png", "", sizes));

  SetDisplayDeviceScaleFactor(1.0f);
  GURL url = FindBestMatchingIconWithMinimum(icons, 1);
  EXPECT_EQ("http://foo.com/icon_x1.png", url.spec());
}

TEST_F(ManifestIconSelectorTest, NotSquareIconsAreIgnored) {
  std::vector<gfx::Size> sizes;
  sizes.push_back(gfx::Size(1024, 1023));

  std::vector<content::Manifest::Icon> icons;
  icons.push_back(CreateIcon("http://foo.com/icon.png", "", sizes));

  GURL url = FindBestMatchingIcon(icons);
  EXPECT_TRUE(url.is_empty());
}

TEST_F(ManifestIconSelectorTest, ClosestIconToPreferred) {
  // Ensure ManifestIconSelector::FindBestMatchingIcon selects the closest icon
  // to the preferred size when presented with a number of options.
  int very_small = GetPreferredIconSizeInDp() / 4;
  int small_size = GetPreferredIconSizeInDp() / 2;
  int bit_small = GetPreferredIconSizeInDp() - 1;
  int bit_big = GetPreferredIconSizeInDp() + 1;
  int big = GetPreferredIconSizeInDp() * 2;
  int very_big = GetPreferredIconSizeInDp() * 4;

  SetDisplayDeviceScaleFactor(1.0f);
  // (very_small, bit_small) => bit_small
  {
    std::vector<gfx::Size> sizes_1;
    sizes_1.push_back(gfx::Size(very_small, very_small));

    std::vector<gfx::Size> sizes_2;
    sizes_2.push_back(gfx::Size(bit_small, bit_small));

    std::vector<content::Manifest::Icon> icons;
    icons.push_back(CreateIcon("http://foo.com/icon_no.png", "", sizes_1));
    icons.push_back(CreateIcon("http://foo.com/icon.png", "", sizes_2));

    GURL url = FindBestMatchingIcon(icons);
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

    GURL url = FindBestMatchingIcon(icons);
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

    GURL url = FindBestMatchingIcon(icons);
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

    GURL url = FindBestMatchingIcon(icons);
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

    GURL url = FindBestMatchingIcon(icons);
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

    GURL url = FindBestMatchingIcon(icons);
    EXPECT_EQ("http://foo.com/icon.png", url.spec());
  }
}

TEST_F(ManifestIconSelectorTest, UseAnyIfNoPreferredSize) {
  // 'any' (ie. gfx::Size(0,0)) should be used if there is no icon of a
  // preferred size. An icon with the current device scale factor is preferred
  // over one with the default density.

  // Icon with 'any' and icon with preferred size => preferred size is chosen.
  {
    std::vector<gfx::Size> sizes_1;
    sizes_1.push_back(gfx::Size(GetPreferredIconSizeInDp(),
                                GetPreferredIconSizeInDp()));
    std::vector<gfx::Size> sizes_2;
    sizes_2.push_back(gfx::Size(0, 0));

    std::vector<content::Manifest::Icon> icons;
    icons.push_back(CreateIcon("http://foo.com/icon.png", "", sizes_1));
    icons.push_back(CreateIcon("http://foo.com/icon_no.png", "", sizes_2));

    GURL url = FindBestMatchingIcon(icons);
    EXPECT_EQ("http://foo.com/icon.png", url.spec());
  }

  // Icon with 'any' and icon larger than preferred size => any is chosen.
  {
    std::vector<gfx::Size> sizes_1;
    sizes_1.push_back(gfx::Size(GetPreferredIconSizeInDp() + 1,
                                GetPreferredIconSizeInDp() + 1));
    std::vector<gfx::Size> sizes_2;
    sizes_2.push_back(gfx::Size(0, 0));

    std::vector<content::Manifest::Icon> icons;
    icons.push_back(CreateIcon("http://foo.com/icon_no.png", "", sizes_1));
    icons.push_back(CreateIcon("http://foo.com/icon.png", "", sizes_2));

    GURL url = FindBestMatchingIcon(icons);
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

    SetDisplayDeviceScaleFactor(3.0f);
    GURL url = FindBestMatchingIcon(icons);
    EXPECT_EQ("http://foo.com/icon.png", url.spec());
  }
}
