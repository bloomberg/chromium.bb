// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/search/instant_service.h"
#include "chrome/browser/search/instant_service_factory.h"
#include "chrome/browser/search/instant_service_observer.h"
#include "chrome/browser/search/ntp_features.h"
#include "chrome/browser/search/search.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/search/instant_test_utils.h"
#include "chrome/browser/ui/search/local_ntp_test_utils.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/search/instant_types.h"
#include "chrome/common/url_constants.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/web_contents.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class TestThemeInfoObserver : public InstantServiceObserver {
 public:
  explicit TestThemeInfoObserver(InstantService* service) : service_(service) {
    service_->AddObserver(this);
  }

  ~TestThemeInfoObserver() override { service_->RemoveObserver(this); }

  void WaitForThemeInfoUpdated(std::string background_url,
                               std::string attribution_1,
                               std::string attribution_2,
                               std::string attribution_action_url) {
    DCHECK(!quit_closure_);

    expected_background_url_ = background_url;
    expected_attribution_1_ = attribution_1;
    expected_attribution_2_ = attribution_2;
    expected_attribution_action_url_ = attribution_action_url;

    if (theme_info_.custom_background_url == background_url &&
        theme_info_.custom_background_attribution_line_1 == attribution_1 &&
        theme_info_.custom_background_attribution_line_2 == attribution_2 &&
        theme_info_.custom_background_attribution_action_url ==
            attribution_action_url) {
      return;
    }

    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  void ThemeInfoChanged(const ThemeBackgroundInfo& theme_info) override {
    theme_info_ = theme_info;

    if (quit_closure_ &&
        theme_info_.custom_background_url == expected_background_url_ &&
        theme_info_.custom_background_attribution_line_1 ==
            expected_attribution_1_ &&
        theme_info_.custom_background_attribution_line_2 ==
            expected_attribution_2_ &&
        theme_info_.custom_background_attribution_action_url ==
            expected_attribution_action_url_) {
      std::move(quit_closure_).Run();
      quit_closure_.Reset();
    }
  }

  void MostVisitedItemsChanged(const std::vector<InstantMostVisitedItem>&,
                               bool is_custom_links) override {}

  InstantService* const service_;

  ThemeBackgroundInfo theme_info_;

  std::string expected_background_url_;
  std::string expected_attribution_1_;
  std::string expected_attribution_2_;
  std::string expected_attribution_action_url_;
  base::OnceClosure quit_closure_;
};

class LocalNTPCustomBackgroundsTest : public InProcessBrowserTest {
 public:
  LocalNTPCustomBackgroundsTest() {
    feature_list_.InitWithFeatures(
        {features::kUseGoogleLocalNtp, features::kNtpBackgrounds}, {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(LocalNTPCustomBackgroundsTest,
                       EmbeddedSearchAPIEndToEnd) {
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));

  TestThemeInfoObserver observer(
      InstantServiceFactory::GetForProfile(browser()->profile()));

  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser());

  // Check that a URL with no attributions can be set.
  EXPECT_TRUE(content::ExecuteScript(active_tab,
                                     "window.chrome.embeddedSearch.newTabPage."
                                     "setBackgroundURL('https://www.test.com/"
                                     "')"));
  observer.WaitForThemeInfoUpdated("https://www.test.com/", "", "", "");

  // Check that a URL with attributions can be set.
  EXPECT_TRUE(content::ExecuteScript(active_tab,
                                     "window.chrome.embeddedSearch.newTabPage."
                                     "setBackgroundURLWithAttributions('https:/"
                                     "/www.test.com/', 'attr1', 'attr2', "
                                     "'https://www.attribution.com/')"));
  observer.WaitForThemeInfoUpdated("https://www.test.com/", "attr1", "attr2",
                                   "https://www.attribution.com/");

  // Setting the background URL to an empty string should clear everything.
  EXPECT_TRUE(content::ExecuteScript(
      active_tab,
      "window.chrome.embeddedSearch.newTabPage.setBackgroundURL('')"));
  observer.WaitForThemeInfoUpdated("", "", "", "");
}

IN_PROC_BROWSER_TEST_F(LocalNTPCustomBackgroundsTest, SetandReset) {
  content::WebContents* active_tab =
      local_ntp_test_utils::OpenNewTab(browser(), GURL("about:blank"));

  TestThemeInfoObserver observer(
      InstantServiceFactory::GetForProfile(browser()->profile()));

  local_ntp_test_utils::NavigateToNTPAndWaitUntilLoaded(browser());

  // Set a custom background image via the EmbeddedSearch API.
  EXPECT_TRUE(content::ExecuteScript(
      active_tab,
      "window.chrome.embeddedSearch.newTabPage."
      "setBackgroundURL('chrome-search://local-ntp/background1.jpg"
      "')"));
  observer.WaitForThemeInfoUpdated("chrome-search://local-ntp/background1.jpg",
                                   "", "", "");

  // Check that the custom background element has the correct image with
  // the scrim applied.
  bool result = false;
  EXPECT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab,
      "$('custom-bg').style.backgroundImage == 'linear-gradient(rgba(0, 0, 0, "
      "0), rgba(0, 0, 0, 0.3)), "
      "url(\"chrome-search://local-ntp/background1.jpg\")'",
      &result));
  EXPECT_TRUE(result);

  // Clear the custom background image via the EmbeddedSearch API.
  EXPECT_TRUE(content::ExecuteScript(active_tab,
                                     "window.chrome.embeddedSearch.newTabPage."
                                     "setBackgroundURL('')"));
  observer.WaitForThemeInfoUpdated("", "", "", "");

  // Check that the custom background was cleared.
  EXPECT_TRUE(instant_test_utils::GetBoolFromJS(
      active_tab, "$('custom-bg').backgroundImage == ''", &result));
  EXPECT_FALSE(result);
}

}  // namespace
