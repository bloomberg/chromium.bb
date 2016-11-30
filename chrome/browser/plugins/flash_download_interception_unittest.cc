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

  bool ShouldStopFlashDownloadAction(const std::string& target_url) {
    return FlashDownloadInterception::ShouldStopFlashDownloadAction(
        host_content_settings_map(), GURL("https://source-url.com/"),
        GURL(target_url), true);
  }
};

TEST_F(FlashDownloadInterceptionTest, PreferHtmlOverPluginsOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kPreferHtmlOverPlugins);

  EXPECT_FALSE(
      ShouldStopFlashDownloadAction("https://get.adobe.com/flashplayer/"));
}

TEST_F(FlashDownloadInterceptionTest, DownloadUrlVariations) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kPreferHtmlOverPlugins);

  const char* flash_intercept_urls[] = {
      "https://get.adobe.com/flashplayer/",
      "http://get.adobe.com/flash",
      "http://get.adobe.com/fr/flashplayer/",
      "http://get.adobe.com/flashplayer",
      "http://macromedia.com/go/getflashplayer",
      "http://adobe.com/go/getflashplayer",
      "http://adobe.com/go/gntray_dl_getflashplayer_jp",
  };

  for (auto& url : flash_intercept_urls) {
    EXPECT_TRUE(ShouldStopFlashDownloadAction(url))
        << "Should have intercepted: " << url;
  }

  const char* flash_no_intercept_urls[] = {
      "https://www.example.com",
      "http://example.com/get.adobe.com/flashplayer",
      "http://ww.macromedia.com/go/getflashplayer",
      "http://wwwxmacromedia.com/go/getflashplayer",
      "http://www.adobe.com/software/flash/about/",
      "http://www.adobe.com/products/flashplayer.html",
      "http://www.adobe.com/products/flashruntimes.html",
      "http://www.adobe.com/go/flash",
  };

  for (auto& url : flash_no_intercept_urls) {
    EXPECT_FALSE(ShouldStopFlashDownloadAction(url))
        << "Should not have intercepted: " << url;
  }

  // Don't intercept navigations occurring on the flash download page.
  EXPECT_FALSE(FlashDownloadInterception::ShouldStopFlashDownloadAction(
      host_content_settings_map(), GURL("https://get.adobe.com/flashplayer/"),
      GURL("https://get.adobe.com/flashplayer/"), true));
}

TEST_F(FlashDownloadInterceptionTest, NavigationThrottleCancelsNavigation) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kPreferHtmlOverPlugins);

  // Set the source URL to an HTTP source.
  NavigateAndCommit(GURL("http://example.com"));

  std::unique_ptr<NavigationHandle> handle =
      NavigationHandle::CreateNavigationHandleForTesting(
          GURL("https://get.adobe.com/flashplayer"), main_rfh(),
          false /* committed */, net::OK, true /* has_user_gesture */);

  handle->CallWillStartRequestForTesting(true, content::Referrer(),
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
  EXPECT_TRUE(
      ShouldStopFlashDownloadAction("https://get.adobe.com/flashplayer/"));

  // No intercept on ALLOW.
  HostContentSettingsMap* map =
      HostContentSettingsMapFactory::GetForProfile(profile());
  map->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_PLUGINS,
                                CONTENT_SETTING_ALLOW);
  EXPECT_FALSE(
      ShouldStopFlashDownloadAction("https://get.adobe.com/flashplayer/"));

  // Intercept on both explicit DETECT and BLOCK.
  map->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_PLUGINS,
                                CONTENT_SETTING_BLOCK);
  EXPECT_TRUE(
      ShouldStopFlashDownloadAction("https://get.adobe.com/flashplayer/"));
  map->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_PLUGINS,
                                CONTENT_SETTING_DETECT_IMPORTANT_CONTENT);
  EXPECT_TRUE(
      ShouldStopFlashDownloadAction("https://get.adobe.com/flashplayer/"));
}
