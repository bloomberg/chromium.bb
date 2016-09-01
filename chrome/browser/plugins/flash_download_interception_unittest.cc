// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/plugins/flash_download_interception.h"

#include "base/test/scoped_feature_list.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/public/browser/navigation_handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

using content::NavigationHandle;
using content::NavigationThrottle;

class FlashDownloadInterceptionTest : public ChromeRenderViewHostTestHarness {
 public:
  FlashDownloadInterceptionTest() {}

  void SetUp() override {
    ChromeRenderViewHostTestHarness::SetUp();
    flash_navigation_handle_ =
        NavigationHandle::CreateNavigationHandleForTesting(
            GURL("https://get.adobe.com/flashplayer"), main_rfh());
  }

  void TearDown() override {
    flash_navigation_handle_.reset();
    ChromeRenderViewHostTestHarness::TearDown();
  }

 protected:
  std::unique_ptr<NavigationHandle> flash_navigation_handle_;
};

TEST_F(FlashDownloadInterceptionTest, PreferHtmlOverPluginsOff) {
  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndDisableFeature(features::kPreferHtmlOverPlugins);

  flash_navigation_handle_->CallWillStartRequestForTesting(
      true, content::Referrer(), true, ui::PAGE_TRANSITION_LINK, false);
  std::unique_ptr<NavigationThrottle> throttle =
      FlashDownloadInterception::MaybeCreateThrottleFor(
          flash_navigation_handle_.get());
  EXPECT_EQ(nullptr, throttle);
}

TEST_F(FlashDownloadInterceptionTest, PreferHtmlOverPluginsOn) {
  std::unique_ptr<NavigationThrottle> throttle;
  std::unique_ptr<NavigationHandle> flash_slash_navigation_handle =
        NavigationHandle::CreateNavigationHandleForTesting(
            GURL("https://get.adobe.com/flashplayer/"), main_rfh());
  std::unique_ptr<NavigationHandle> example_navigation_handle =
        NavigationHandle::CreateNavigationHandleForTesting(
            GURL("https://www.example.com"), main_rfh());
  std::unique_ptr<NavigationHandle> flash_http_navigation_handle =
        NavigationHandle::CreateNavigationHandleForTesting(
            GURL("http://get.adobe.com/flashplayer"), main_rfh());
  std::unique_ptr<NavigationHandle> example_flash_navigation_handle =
        NavigationHandle::CreateNavigationHandleForTesting(
            GURL("http://example.com/get.adobe.com/flashplayer"), main_rfh());

  base::test::ScopedFeatureList feature_list;
  feature_list.InitAndEnableFeature(features::kPreferHtmlOverPlugins);

  flash_navigation_handle_->CallWillStartRequestForTesting(
      true, content::Referrer(), false, ui::PAGE_TRANSITION_LINK, false);
  throttle = FlashDownloadInterception::MaybeCreateThrottleFor(
      flash_navigation_handle_.get());
  EXPECT_EQ(nullptr, throttle);

  flash_navigation_handle_->CallWillStartRequestForTesting(
      true, content::Referrer(), true, ui::PAGE_TRANSITION_LINK, false);
  throttle = FlashDownloadInterception::MaybeCreateThrottleFor(
      flash_navigation_handle_.get());
  EXPECT_NE(nullptr, throttle);
  ASSERT_EQ(NavigationThrottle::CANCEL_AND_IGNORE,
            throttle->WillStartRequest());

  flash_slash_navigation_handle->CallWillStartRequestForTesting(
      true, content::Referrer(), true, ui::PAGE_TRANSITION_LINK, false);
  throttle = FlashDownloadInterception::MaybeCreateThrottleFor(
      flash_slash_navigation_handle.get());
  EXPECT_NE(nullptr, throttle);
  ASSERT_EQ(NavigationThrottle::CANCEL_AND_IGNORE,
            throttle->WillStartRequest());

  example_navigation_handle->CallWillStartRequestForTesting(
      true, content::Referrer(), true, ui::PAGE_TRANSITION_LINK, false);
  throttle = FlashDownloadInterception::MaybeCreateThrottleFor(
      example_navigation_handle.get());
  EXPECT_EQ(nullptr, throttle);

  flash_http_navigation_handle->CallWillStartRequestForTesting(
      true, content::Referrer(), true, ui::PAGE_TRANSITION_LINK, false);
  throttle = FlashDownloadInterception::MaybeCreateThrottleFor(
      flash_http_navigation_handle.get());
  EXPECT_NE(nullptr, throttle);
  ASSERT_EQ(NavigationThrottle::CANCEL_AND_IGNORE,
            throttle->WillStartRequest());

  example_flash_navigation_handle->CallWillStartRequestForTesting(
      true, content::Referrer(), true, ui::PAGE_TRANSITION_LINK, false);
  throttle = FlashDownloadInterception::MaybeCreateThrottleFor(
      example_flash_navigation_handle.get());
  EXPECT_EQ(nullptr, throttle);
}
