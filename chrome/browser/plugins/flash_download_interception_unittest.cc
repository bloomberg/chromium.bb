// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/flash_download_interception.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "chrome/test/base/testing_profile.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "content/public/browser/navigation_handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using content::NavigationHandle;
using content::NavigationThrottle;

class FlashDownloadInterceptionTest : public ChromeRenderViewHostTestHarness {
 public:
  HostContentSettingsMap* host_content_settings_map() {
    return HostContentSettingsMapFactory::GetForProfile(profile());
  }
};

TEST_F(FlashDownloadInterceptionTest, PreferHtmlOverPluginsOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kPreferHtmlOverPlugins);

  EXPECT_FALSE(FlashDownloadInterception::ShouldStopFlashDownloadAction(
      host_content_settings_map(), GURL("http://source-page.com"),
      GURL("https://get.adobe.com/flashplayer/"), true));
}

TEST_F(FlashDownloadInterceptionTest, DownloadUrlVariations) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kPreferHtmlOverPlugins);

  EXPECT_TRUE(FlashDownloadInterception::ShouldStopFlashDownloadAction(
      host_content_settings_map(), GURL("http://source-page.com"),
      GURL("https://get.adobe.com/flashplayer/"), true));

  // HTTP and path variant.
  EXPECT_TRUE(FlashDownloadInterception::ShouldStopFlashDownloadAction(
      host_content_settings_map(), GURL("http://source-page.com"),
      GURL("http://get.adobe.com/flash"), true));

  EXPECT_TRUE(FlashDownloadInterception::ShouldStopFlashDownloadAction(
      host_content_settings_map(), GURL("http://source-page.com"),
      GURL("http://www.macromedia.com/go/getflashplayer"), true));

  EXPECT_TRUE(FlashDownloadInterception::ShouldStopFlashDownloadAction(
      host_content_settings_map(), GURL("http://source-page.com"),
      GURL("http://macromedia.com/go/getflashplayer"), true));

  EXPECT_FALSE(FlashDownloadInterception::ShouldStopFlashDownloadAction(
      host_content_settings_map(), GURL("http://source-page.com"),
      GURL("https://www.example.com"), true));

  EXPECT_FALSE(FlashDownloadInterception::ShouldStopFlashDownloadAction(
      host_content_settings_map(), GURL("http://source-page.com"),
      GURL("http://example.com/get.adobe.com/flashplayer"), true));

  EXPECT_FALSE(FlashDownloadInterception::ShouldStopFlashDownloadAction(
      host_content_settings_map(), GURL("http://source-page.com"),
      GURL("http://ww.macromedia.com/go/getflashplayer"), true));
}

TEST_F(FlashDownloadInterceptionTest, NavigationThrottleCancelsNavigation) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kPreferHtmlOverPlugins);

  // Set the source URL to an HTTP source.
  NavigateAndCommit(GURL("http://example.com"));

  std::unique_ptr<NavigationHandle> handle =
      NavigationHandle::CreateNavigationHandleForTesting(
          GURL("https://get.adobe.com/flashplayer"), main_rfh());

  handle->CallWillStartRequestForTesting(true, content::Referrer(), true,
                                         ui::PAGE_TRANSITION_LINK, false);
  std::unique_ptr<NavigationThrottle> throttle =
      FlashDownloadInterception::MaybeCreateThrottleFor(handle.get());
  EXPECT_NE(nullptr, throttle);
  ASSERT_EQ(NavigationThrottle::CANCEL_AND_IGNORE,
            throttle->WillStartRequest());
}

TEST_F(FlashDownloadInterceptionTest, OnlyInterceptOnDetectContentSetting) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kPreferHtmlOverPlugins);

  // Default Setting (which is DETECT)
  EXPECT_TRUE(FlashDownloadInterception::ShouldStopFlashDownloadAction(
      host_content_settings_map(), GURL("http://source-page.com"),
      GURL("https://get.adobe.com/flashplayer/"), true));

  // No intercept on ALLOW.
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  map->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_PLUGINS,
                                CONTENT_SETTING_ALLOW);
  EXPECT_FALSE(FlashDownloadInterception::ShouldStopFlashDownloadAction(
      host_content_settings_map(), GURL("http://source-page.com"),
      GURL("https://get.adobe.com/flashplayer/"), true));

  // Intercept on both explicit DETECT and BLOCK.
  map->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_PLUGINS,
                                CONTENT_SETTING_BLOCK);
  EXPECT_TRUE(FlashDownloadInterception::ShouldStopFlashDownloadAction(
      host_content_settings_map(), GURL("http://source-page.com"),
      GURL("https://get.adobe.com/flashplayer/"), true));
  map->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_PLUGINS,
                                CONTENT_SETTING_DETECT_IMPORTANT_CONTENT);
  EXPECT_TRUE(FlashDownloadInterception::ShouldStopFlashDownloadAction(
      host_content_settings_map(), GURL("http://source-page.com"),
      GURL("https://get.adobe.com/flashplayer/"), true));
}
