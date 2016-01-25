// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/banners/app_banner_data_fetcher.h"

#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace banners {

class AppBannerDataFetcherUnitTest : public testing::Test {
 public:
  AppBannerDataFetcherUnitTest() { }

 protected:
  static base::NullableString16 ToNullableUTF16(const std::string& str) {
    return base::NullableString16(base::UTF8ToUTF16(str), false);
  }

  static content::Manifest GetValidManifest() {
    content::Manifest manifest;
    manifest.name = ToNullableUTF16("foo");
    manifest.short_name = ToNullableUTF16("bar");
    manifest.start_url = GURL("http://example.com");

    content::Manifest::Icon icon;
    icon.type = ToNullableUTF16("image/png");
    icon.sizes.push_back(gfx::Size(144, 144));
    manifest.icons.push_back(icon);

    return manifest;
  }

  static bool IsManifestValid(const content::Manifest& manifest) {
    // The second argument is the web_contents pointer, which is used for
    // developer debug logging to the console. The logging is skipped inside the
    // method if a null web_contents pointer is provided, so this is safe.
    // The third argument is the is_debug_mode boolean value, which is true only
    // when it is triggered by the developer's action in DevTools.
    return AppBannerDataFetcher::IsManifestValidForWebApp(manifest, nullptr,
                                                          false);
  }
};

TEST_F(AppBannerDataFetcherUnitTest, EmptyManifestIsInvalid) {
  content::Manifest manifest;
  EXPECT_FALSE(IsManifestValid(manifest));
}

TEST_F(AppBannerDataFetcherUnitTest, CheckMinimalValidManifest) {
  content::Manifest manifest = GetValidManifest();
  EXPECT_TRUE(IsManifestValid(manifest));
}

TEST_F(AppBannerDataFetcherUnitTest, ManifestRequiresNameORShortName) {
  content::Manifest manifest = GetValidManifest();

  manifest.name = base::NullableString16();
  EXPECT_TRUE(IsManifestValid(manifest));

  manifest.name = ToNullableUTF16("foo");
  manifest.short_name = base::NullableString16();
  EXPECT_TRUE(IsManifestValid(manifest));

  manifest.name = base::NullableString16();
  EXPECT_FALSE(IsManifestValid(manifest));
}

TEST_F(AppBannerDataFetcherUnitTest, ManifestRequiresValidStartURL) {
  content::Manifest manifest = GetValidManifest();

  manifest.start_url = GURL();
  EXPECT_FALSE(IsManifestValid(manifest));

  manifest.start_url = GURL("/");
  EXPECT_FALSE(IsManifestValid(manifest));
}

TEST_F(AppBannerDataFetcherUnitTest, ManifestRequiresImagePNG) {
  content::Manifest manifest = GetValidManifest();

  manifest.icons[0].type = ToNullableUTF16("image/gif");
  EXPECT_FALSE(IsManifestValid(manifest));
  manifest.icons[0].type = base::NullableString16();
  EXPECT_FALSE(IsManifestValid(manifest));
}

TEST_F(AppBannerDataFetcherUnitTest, ManifestRequiresMinimalSize) {
  content::Manifest manifest = GetValidManifest();

  // The icon MUST be 144x144 size at least.
  manifest.icons[0].sizes[0] = gfx::Size(1, 1);
  EXPECT_FALSE(IsManifestValid(manifest));

  // If one of the sizes match the requirement, it should be accepted.
  manifest.icons[0].sizes.push_back(gfx::Size(144, 144));
  EXPECT_TRUE(IsManifestValid(manifest));

  // Higher than the required size is okay.
  manifest.icons[0].sizes[1] = gfx::Size(200, 200);
  EXPECT_TRUE(IsManifestValid(manifest));

  // Non-square is okay.
  manifest.icons[0].sizes[1] = gfx::Size(144, 200);
  EXPECT_TRUE(IsManifestValid(manifest));

  // The representation of the keyword 'any' should be recognized.
  manifest.icons[0].sizes[1] = gfx::Size(0, 0);
  EXPECT_TRUE(IsManifestValid(manifest));
}

}  // namespace banners
