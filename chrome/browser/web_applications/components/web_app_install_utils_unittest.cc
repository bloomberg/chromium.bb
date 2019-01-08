// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/web_applications/components/web_app_install_utils.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/common/web_application_info.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/manifest/manifest.h"

namespace web_app {

namespace {
const char kAppIcon1[] = "fav1.png";
const char kAppIcon2[] = "fav2.png";
const char kAppIcon3[] = "fav3.png";
const char kAppShortName[] = "Test short name";
const char kAppTitle[] = "Test title";
const char kAppUrl[] = "http://www.chromium.org/index.html";
const char kAlternativeAppUrl[] = "http://www.notchromium.org";
const char kAlternativeAppTitle[] = "Different test title";
}  // namespace

TEST(WebAppInstallUtils, UpdateWebAppInfoFromManifest) {
  WebApplicationInfo web_app_info;
  web_app_info.title = base::UTF8ToUTF16(kAlternativeAppTitle);
  web_app_info.app_url = GURL(kAlternativeAppUrl);
  WebApplicationInfo::IconInfo info;
  info.url = GURL(kAppIcon1);
  web_app_info.icons.push_back(info);

  blink::Manifest manifest;
  manifest.start_url = GURL(kAppUrl);
  manifest.short_name =
      base::NullableString16(base::UTF8ToUTF16(kAppShortName), false);

  UpdateWebAppInfoFromManifest(manifest, &web_app_info,
                               ForInstallableSite::kNo);
  EXPECT_EQ(base::UTF8ToUTF16(kAppShortName), web_app_info.title);
  EXPECT_EQ(GURL(kAppUrl), web_app_info.app_url);

  // The icon info from |web_app_info| should be left as is, since the manifest
  // doesn't have any icon information.
  EXPECT_EQ(1u, web_app_info.icons.size());
  EXPECT_EQ(GURL(kAppIcon1), web_app_info.icons[0].url);

  // Test that |manifest.name| takes priority over |manifest.short_name|, and
  // that icons provided by the manifest replace icons in |web_app_info|.
  manifest.name = base::NullableString16(base::UTF8ToUTF16(kAppTitle), false);

  blink::Manifest::ImageResource icon;
  icon.src = GURL(kAppIcon2);
  icon.purpose = {blink::Manifest::ImageResource::Purpose::ANY,
                  blink::Manifest::ImageResource::Purpose::BADGE};
  manifest.icons.push_back(icon);
  icon.src = GURL(kAppIcon3);
  manifest.icons.push_back(icon);
  // Add an icon without purpose ANY (expect to be ignored).
  icon.purpose = {blink::Manifest::ImageResource::Purpose::BADGE};
  manifest.icons.push_back(icon);

  UpdateWebAppInfoFromManifest(manifest, &web_app_info,
                               ForInstallableSite::kNo);
  EXPECT_EQ(base::UTF8ToUTF16(kAppTitle), web_app_info.title);

  EXPECT_EQ(2u, web_app_info.icons.size());
  EXPECT_EQ(GURL(kAppIcon2), web_app_info.icons[0].url);
  EXPECT_EQ(GURL(kAppIcon3), web_app_info.icons[1].url);
}

// Tests "scope" is only set for installable sites.
TEST(WebAppInstallUtils, UpdateWebAppInfoFromManifestInstallableSite) {
  {
    blink::Manifest manifest;
    manifest.start_url = GURL(kAppUrl);
    WebApplicationInfo web_app_info;
    UpdateWebAppInfoFromManifest(manifest, &web_app_info,
                                 ForInstallableSite::kUnknown);
    EXPECT_EQ(GURL(), web_app_info.scope);
  }

  {
    blink::Manifest manifest;
    manifest.start_url = GURL(kAppUrl);
    WebApplicationInfo web_app_info;
    UpdateWebAppInfoFromManifest(manifest, &web_app_info,
                                 ForInstallableSite::kNo);
    EXPECT_EQ(GURL(), web_app_info.scope);
  }

  {
    blink::Manifest manifest;
    manifest.start_url = GURL(kAppUrl);
    WebApplicationInfo web_app_info;
    UpdateWebAppInfoFromManifest(manifest, &web_app_info,
                                 ForInstallableSite::kYes);

    EXPECT_NE(GURL(), web_app_info.scope);
  }
}

}  // namespace web_app
