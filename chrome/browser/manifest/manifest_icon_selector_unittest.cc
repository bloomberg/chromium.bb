// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/manifest/manifest_icon_selector.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/gfx/screen.h"
#include "ui/gfx/screen_type_delegate.h"

namespace {

const int kPreferredIconSize = 48;

}

// A dummy implementation of gfx::Screen, since ManifestIconSelector needs
// access to a gfx::Display's device scale factor.
// This is inspired by web_contents_video_capture_device_unittest.cc
// A bug has been opened to merge all those mocks: http://crbug.com/417227
class FakeScreen : public gfx::Screen {
 public:
  FakeScreen() : display_(0x1337, gfx::Rect(0, 0, 2560, 1440)) {
  }
  ~FakeScreen() override {}

  void SetDisplayDeviceScaleFactor(float device_scale_factor) {
    display_.set_device_scale_factor(device_scale_factor);
  }

  // gfx::Screen implementation (only what's needed for testing).
  gfx::Point GetCursorScreenPoint() override { return gfx::Point(); }
  gfx::NativeWindow GetWindowUnderCursor() override { return nullptr; }
  gfx::NativeWindow GetWindowAtScreenPoint(
      const gfx::Point& point) override { return nullptr; }
  int GetNumDisplays() const override { return 1; }
  std::vector<gfx::Display> GetAllDisplays() const override {
    return std::vector<gfx::Display>(1, display_);
  }
  gfx::Display GetDisplayNearestWindow(
      gfx::NativeView view) const override {
    return display_;
  }
  gfx::Display GetDisplayNearestPoint(
      const gfx::Point& point) const override {
    return display_;
  }
  gfx::Display GetDisplayMatching(
      const gfx::Rect& match_rect) const override {
    return display_;
  }
  gfx::Display GetPrimaryDisplay() const override {
    return display_;
  }
  void AddObserver(gfx::DisplayObserver* observer) override {}
  void RemoveObserver(gfx::DisplayObserver* observer) override {}

 private:
  gfx::Display display_;

  DISALLOW_COPY_AND_ASSIGN(FakeScreen);
};

class ManifestIconSelectorTest : public testing::Test  {
 protected:
  ManifestIconSelectorTest() {}
  ~ManifestIconSelectorTest() override {}

  GURL FindBestMatchingIcon(const std::vector<content::Manifest::Icon>& icons) {
    return ManifestIconSelector::FindBestMatchingIcon(
        icons,
        GetPreferredIconSizeInDp(),
        &fake_screen_);
  }

  void SetDisplayDeviceScaleFactor(float device_scale_factor) {
    fake_screen_.SetDisplayDeviceScaleFactor(device_scale_factor);
  }

  static int GetPreferredIconSizeInDp() {
    return kPreferredIconSize;
  }

  static content::Manifest::Icon CreateIcon(
      const std::string& url,
      const std::string& type,
      double density,
      const std::vector<gfx::Size> sizes) {
    content::Manifest::Icon icon;
    icon.src = GURL(url);
    if (!type.empty())
      icon.type = base::NullableString16(base::UTF8ToUTF16(type), false);
    icon.density = density;
    icon.sizes = sizes;

    return icon;
  }

 private:
  FakeScreen fake_screen_;

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
      CreateIcon("http://foo.com/icon.png", "", 1.0, std::vector<gfx::Size>()));

  GURL url = FindBestMatchingIcon(icons);
  EXPECT_TRUE(url.is_empty());
}

TEST_F(ManifestIconSelectorTest, MIMETypeFiltering) {
  // Icons with type specified to a MIME type that isn't a valid image MIME type
  // are ignored.
  std::vector<gfx::Size> sizes;
  sizes.push_back(gfx::Size(10, 10));

  std::vector<content::Manifest::Icon> icons;
  icons.push_back(
      CreateIcon("http://foo.com/icon.png", "image/foo_bar", 1.0, sizes));
  icons.push_back(CreateIcon("http://foo.com/icon.png", "image/", 1.0, sizes));
  icons.push_back(CreateIcon("http://foo.com/icon.png", "image/", 1.0, sizes));
  icons.push_back(
      CreateIcon("http://foo.com/icon.png", "video/mp4", 1.0, sizes));

  GURL url = FindBestMatchingIcon(icons);
  EXPECT_TRUE(url.is_empty());

  icons.clear();
  icons.push_back(
      CreateIcon("http://foo.com/icon.png", "image/png", 1.0, sizes));
  url = FindBestMatchingIcon(icons);
  EXPECT_EQ("http://foo.com/icon.png", url.spec());

  icons.clear();
  icons.push_back(
      CreateIcon("http://foo.com/icon.png", "image/gif", 1.0, sizes));
  url = FindBestMatchingIcon(icons);
  EXPECT_EQ("http://foo.com/icon.png", url.spec());

  icons.clear();
  icons.push_back(
      CreateIcon("http://foo.com/icon.png", "image/jpeg", 1.0, sizes));
  url = FindBestMatchingIcon(icons);
  EXPECT_EQ("http://foo.com/icon.png", url.spec());
}

TEST_F(ManifestIconSelectorTest, PreferredSizeOfCurrentDensityIsUsedFirst) {
  // This test has three icons each are marked with sizes set to the preferred
  // icon size for the associated density.
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
  icons.push_back(CreateIcon("http://foo.com/icon_x1.png", "", 1.0, sizes_1));
  icons.push_back(CreateIcon("http://foo.com/icon_x2.png", "", 2.0, sizes_2));
  icons.push_back(CreateIcon("http://foo.com/icon_x3.png", "", 3.0, sizes_3));

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

TEST_F(ManifestIconSelectorTest, PreferredSizeOfDefaultDensityIsUsedSecond) {
  // This test has three icons. The first one is of density zero and is marked
  // with three sizes which are the preferred icon size for density 1, 2 and 3.
  // The icon for density 2 and 3 have a size set to 2x2 and 3x3.
  // Regardless of the device scale factor, the icon of density 1 is going to be
  // used because it matches the preferred size.
  std::vector<gfx::Size> sizes_1;
  sizes_1.push_back(gfx::Size(GetPreferredIconSizeInDp(),
                              GetPreferredIconSizeInDp()));
  sizes_1.push_back(gfx::Size(GetPreferredIconSizeInDp() * 2,
                              GetPreferredIconSizeInDp() * 2));
  sizes_1.push_back(gfx::Size(GetPreferredIconSizeInDp() * 3,
                              GetPreferredIconSizeInDp() * 3));

  std::vector<gfx::Size> sizes_2;
  sizes_2.push_back(gfx::Size(2, 2));

  std::vector<gfx::Size> sizes_3;
  sizes_3.push_back(gfx::Size(3, 3));

  std::vector<content::Manifest::Icon> icons;
  icons.push_back(CreateIcon("http://foo.com/icon_x1.png", "", 1.0, sizes_1));
  icons.push_back(CreateIcon("http://foo.com/icon_x2.png", "", 2.0, sizes_2));
  icons.push_back(CreateIcon("http://foo.com/icon_x3.png", "", 3.0, sizes_3));

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

TEST_F(ManifestIconSelectorTest, DeviceDensityFirst) {
  // If there is no perfect icon but an icon of the current device density is
  // present, it will be picked.
  // This test has three icons each are marked with sizes set to the preferred
  // icon size for the associated density.
  std::vector<gfx::Size> sizes;
  sizes.push_back(gfx::Size(2, 2));

  std::vector<content::Manifest::Icon> icons;
  icons.push_back(CreateIcon("http://foo.com/icon_x1.png", "", 1.0, sizes));
  icons.push_back(CreateIcon("http://foo.com/icon_x2.png", "", 2.0, sizes));
  icons.push_back(CreateIcon("http://foo.com/icon_x3.png", "", 3.0, sizes));

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

TEST_F(ManifestIconSelectorTest, DeviceDensityFallback) {
  // If there is no perfect icon but and no icon of the current display density,
  // an icon of density 1.0 will be used.
  std::vector<gfx::Size> sizes;
  sizes.push_back(gfx::Size(2, 2));

  std::vector<content::Manifest::Icon> icons;
  icons.push_back(CreateIcon("http://foo.com/icon_x1.png", "", 1.0, sizes));
  icons.push_back(CreateIcon("http://foo.com/icon_x2.png", "", 2.0, sizes));

  SetDisplayDeviceScaleFactor(3.0f);
  GURL url = FindBestMatchingIcon(icons);
  EXPECT_EQ("http://foo.com/icon_x1.png", url.spec());
}

TEST_F(ManifestIconSelectorTest, DoNotUseOtherDensities) {
  // If there are only icons of densities that are not the current display
  // density or the default density, they are ignored.
  std::vector<gfx::Size> sizes;
  sizes.push_back(gfx::Size(2, 2));

  std::vector<content::Manifest::Icon> icons;
  icons.push_back(CreateIcon("http://foo.com/icon_x2.png", "", 2.0, sizes));

  SetDisplayDeviceScaleFactor(3.0f);
  GURL url = FindBestMatchingIcon(icons);
  EXPECT_TRUE(url.is_empty());
}

TEST_F(ManifestIconSelectorTest, NotSquareIconsAreIgnored) {
  std::vector<gfx::Size> sizes;
  sizes.push_back(gfx::Size(20, 2));

  std::vector<content::Manifest::Icon> icons;
  icons.push_back(CreateIcon("http://foo.com/icon.png", "", 1.0, sizes));

  GURL url = FindBestMatchingIcon(icons);
  EXPECT_TRUE(url.is_empty());
}

TEST_F(ManifestIconSelectorTest, ClosestIconToPreferred) {
  // This test verifies ManifestIconSelector::FindBestMatchingIcon by passing
  // different icon sizes and checking which one is picked.
  // The Device Scale Factor is 1.0 and the preferred icon size is returned by
  // GetPreferredIconSizeInDp().
  int very_small = GetPreferredIconSizeInDp() / 4;
  int small_size = GetPreferredIconSizeInDp() / 2;
  int bit_small = GetPreferredIconSizeInDp() - 1;
  int bit_big = GetPreferredIconSizeInDp() + 1;
  int big = GetPreferredIconSizeInDp() * 2;
  int very_big = GetPreferredIconSizeInDp() * 4;

  // (very_small, bit_small) => bit_small
  {
    std::vector<gfx::Size> sizes_1;
    sizes_1.push_back(gfx::Size(very_small, very_small));

    std::vector<gfx::Size> sizes_2;
    sizes_2.push_back(gfx::Size(bit_small, bit_small));

    std::vector<content::Manifest::Icon> icons;
    icons.push_back(CreateIcon("http://foo.com/icon_no.png", "", 1.0, sizes_1));
    icons.push_back(CreateIcon("http://foo.com/icon.png", "", 1.0, sizes_2));

    GURL url = FindBestMatchingIcon(icons);
    EXPECT_EQ("http://foo.com/icon.png", url.spec());
  }

  // (very_small, bit_small, smaller) => bit_small
  {
    std::vector<gfx::Size> sizes_1;
    sizes_1.push_back(gfx::Size(very_small, very_small));

    std::vector<gfx::Size> sizes_2;
    sizes_2.push_back(gfx::Size(bit_small, bit_small));

    std::vector<gfx::Size> sizes_3;
    sizes_3.push_back(gfx::Size(small_size, small_size));

    std::vector<content::Manifest::Icon> icons;
    icons.push_back(CreateIcon("http://foo.com/icon_no.png", "", 1.0, sizes_1));
    icons.push_back(CreateIcon("http://foo.com/icon.png", "", 1.0, sizes_2));
    icons.push_back(CreateIcon("http://foo.com/icon_no.png", "", 1.0, sizes_3));

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
    icons.push_back(CreateIcon("http://foo.com/icon_no.png", "", 1.0, sizes_1));
    icons.push_back(CreateIcon("http://foo.com/icon.png", "", 1.0, sizes_2));

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
    icons.push_back(CreateIcon("http://foo.com/icon_no.png", "", 1.0, sizes_1));
    icons.push_back(CreateIcon("http://foo.com/icon_no.png", "", 1.0, sizes_2));
    icons.push_back(CreateIcon("http://foo.com/icon.png", "", 1.0, sizes_3));

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
    icons.push_back(CreateIcon("http://foo.com/icon_no.png", "", 1.0, sizes_1));
    icons.push_back(CreateIcon("http://foo.com/icon.png", "", 1.0, sizes_2));

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
    icons.push_back(CreateIcon("http://foo.com/icon_no.png", "", 1.0, sizes_1));
    icons.push_back(CreateIcon("http://foo.com/icon.png", "", 1.0, sizes_2));

    GURL url = FindBestMatchingIcon(icons);
    EXPECT_EQ("http://foo.com/icon.png", url.spec());
  }
}

TEST_F(ManifestIconSelectorTest, UseAnyIfNoPreferredSize) {
  // 'any' (ie. gfx::Size(0,0)) should be used if there is no icon of a
  // preferred size. An icon with the current device scale factor is preferred
  // over one with the default density.

  // 'any' with preferred size => preferred size
  {
    std::vector<gfx::Size> sizes_1;
    sizes_1.push_back(gfx::Size(GetPreferredIconSizeInDp(),
                                GetPreferredIconSizeInDp()));
    std::vector<gfx::Size> sizes_2;
    sizes_2.push_back(gfx::Size(0,0));

    std::vector<content::Manifest::Icon> icons;
    icons.push_back(CreateIcon("http://foo.com/icon.png", "", 1.0, sizes_1));
    icons.push_back(CreateIcon("http://foo.com/icon_no.png", "", 1.0, sizes_2));

    GURL url = FindBestMatchingIcon(icons);
    EXPECT_EQ("http://foo.com/icon.png", url.spec());
  }

  // 'any' with nearly preferred size => any
  {
    std::vector<gfx::Size> sizes_1;
    sizes_1.push_back(gfx::Size(GetPreferredIconSizeInDp() + 1,
                                GetPreferredIconSizeInDp() + 1));
    std::vector<gfx::Size> sizes_2;
    sizes_2.push_back(gfx::Size(0,0));

    std::vector<content::Manifest::Icon> icons;
    icons.push_back(CreateIcon("http://foo.com/icon_no.png", "", 1.0, sizes_1));
    icons.push_back(CreateIcon("http://foo.com/icon.png", "", 1.0, sizes_2));

    GURL url = FindBestMatchingIcon(icons);
    EXPECT_EQ("http://foo.com/icon.png", url.spec());
  }

  // 'any' on default density and current density => current density.
  {
    std::vector<gfx::Size> sizes;
    sizes.push_back(gfx::Size(0,0));

    std::vector<content::Manifest::Icon> icons;
    icons.push_back(CreateIcon("http://foo.com/icon_no.png", "", 1.0, sizes));
    icons.push_back(CreateIcon("http://foo.com/icon.png", "", 3.0, sizes));

    SetDisplayDeviceScaleFactor(3.0f);
    GURL url = FindBestMatchingIcon(icons);
    EXPECT_EQ("http://foo.com/icon.png", url.spec());
  }
}
