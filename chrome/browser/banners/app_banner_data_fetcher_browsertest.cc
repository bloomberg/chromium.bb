// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/banners/app_banner_data_fetcher.h"

#include "base/command_line.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner.h"
#include "base/test/histogram_tester.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/banners/app_banner_data_fetcher_desktop.h"
#include "chrome/browser/banners/app_banner_metrics.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace banners {

class TestObserver : public AppBannerDataFetcher::Observer {
 public:
  TestObserver(AppBannerDataFetcher* fetcher, base::Closure quit_closure)
      : fetcher_(fetcher),
        quit_closure_(quit_closure) {
    fetcher_->AddObserverForTesting(this);
  }

  virtual ~TestObserver() {
    if (fetcher_)
      fetcher_->RemoveObserverForTesting(this);
  }

  void OnDecidedWhetherToShow(AppBannerDataFetcher* fetcher,
                              bool will_show) override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, quit_closure_);
    ASSERT_FALSE(will_show_.get());
    will_show_.reset(new bool(will_show));
  }

  void OnFetcherDestroyed(AppBannerDataFetcher* fetcher) override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, quit_closure_);
    fetcher_ = nullptr;
  }

  bool will_show() { return will_show_.get() && *will_show_; }

 private:
  AppBannerDataFetcher* fetcher_;
  base::Closure quit_closure_;
  scoped_ptr<bool> will_show_;
};

class AppBannerDataFetcherBrowserTest : public InProcessBrowserTest,
                                        public AppBannerDataFetcher::Delegate {
 public:
  AppBannerDataFetcherBrowserTest() : weak_factory_(this) {
  }

  void SetUpOnMainThread() override {
    AppBannerSettingsHelper::SetEngagementWeights(1, 1);
    AppBannerSettingsHelper::SetTotalEngagementToTrigger(2);
    ASSERT_TRUE(embedded_test_server()->Start());
    InProcessBrowserTest::SetUpOnMainThread();
  }

  bool HandleNonWebApp(const std::string& platform,
                       const GURL& url,
                       const std::string& id,
                       bool is_debug_mode) override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, quit_closure_);
    non_web_platform_ = platform;
    return false;
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
    // Make sure app banners are disabled in the browser, otherwise they will
    // interfere with the test.
    command_line->AppendSwitch(switches::kDisableAddToShelf);
  }

 protected:
  void RunFetcher(const GURL& url,
                  const std::string& expected_non_web_platform,
                  ui::PageTransition transition,
                  bool expected_to_show) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    scoped_refptr<AppBannerDataFetcherDesktop> fetcher(
        new AppBannerDataFetcherDesktop(
            web_contents, weak_factory_.GetWeakPtr(), 128, 128, false));

    base::HistogramTester histograms;
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    scoped_ptr<TestObserver> observer(new TestObserver(fetcher.get(),
                                                       run_loop.QuitClosure()));
    fetcher->Start(url, transition);
    run_loop.Run();

    EXPECT_EQ(expected_non_web_platform, non_web_platform_);
    EXPECT_EQ(expected_to_show, observer->will_show());
    ASSERT_FALSE(fetcher->is_active());

    // If showing the banner, ensure that the minutes histogram is recorded.
    histograms.ExpectTotalCount(banners::kMinutesHistogram,
                                (observer->will_show() ? 1 : 0));
  }

  void RunBannerTest(const std::string& manifest_page,
                     ui::PageTransition transition,
                     unsigned int unshown_repetitions,
                     bool expectation) {
    std::string valid_page(manifest_page);
    GURL test_url = embedded_test_server()->GetURL(valid_page);
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();

    for (unsigned int i = 0; i < unshown_repetitions; ++i) {
      ui_test_utils::NavigateToURL(browser(), test_url);
      RunFetcher(web_contents->GetURL(), std::string(), transition, false);
      AppBannerDataFetcher::SetTimeDeltaForTesting(i+1);
    }

    // On the final loop, check whether the banner triggered or not as expected.
    ui_test_utils::NavigateToURL(browser(), test_url);
    RunFetcher(web_contents->GetURL(), std::string(), transition, expectation);
  }

 private:
  std::string non_web_platform_;
  base::Closure quit_closure_;
  base::WeakPtrFactory<AppBannerDataFetcherBrowserTest> weak_factory_;
};

IN_PROC_BROWSER_TEST_F(AppBannerDataFetcherBrowserTest,
                       WebAppBannerCreatedDirect) {
  RunBannerTest("/banners/manifest_test_page.html", ui::PAGE_TRANSITION_TYPED,
                1, true);
}

IN_PROC_BROWSER_TEST_F(AppBannerDataFetcherBrowserTest,
                       WebAppBannerCreatedDirectLargerTotal) {
  AppBannerSettingsHelper::SetTotalEngagementToTrigger(4);
  RunBannerTest("/banners/manifest_test_page.html", ui::PAGE_TRANSITION_TYPED,
                3, true);
}

IN_PROC_BROWSER_TEST_F(AppBannerDataFetcherBrowserTest,
                       WebAppBannerCreatedDirectSmallerTotal) {
  AppBannerSettingsHelper::SetTotalEngagementToTrigger(1);
  RunBannerTest("/banners/manifest_test_page.html", ui::PAGE_TRANSITION_TYPED,
                0, true);
}

IN_PROC_BROWSER_TEST_F(AppBannerDataFetcherBrowserTest,
                       WebAppBannerCreatedDirectSingle) {
  AppBannerSettingsHelper::SetEngagementWeights(2, 1);
  RunBannerTest("/banners/manifest_test_page.html",
                ui::PAGE_TRANSITION_GENERATED, 0, true);
}

IN_PROC_BROWSER_TEST_F(AppBannerDataFetcherBrowserTest,
                       WebAppBannerCreatedDirectMultiple) {
  AppBannerSettingsHelper::SetEngagementWeights(0.5, 1);
  RunBannerTest("/banners/manifest_test_page.html",
                ui::PAGE_TRANSITION_GENERATED, 3, true);
}

IN_PROC_BROWSER_TEST_F(AppBannerDataFetcherBrowserTest,
                       WebAppBannerCreatedDirectMultipleLargerTotal) {
  AppBannerSettingsHelper::SetEngagementWeights(0.5, 1);
  AppBannerSettingsHelper::SetTotalEngagementToTrigger(3);
  RunBannerTest("/banners/manifest_test_page.html",
                ui::PAGE_TRANSITION_GENERATED, 5, true);
}

IN_PROC_BROWSER_TEST_F(AppBannerDataFetcherBrowserTest,
                       WebAppBannerCreatedDirectMultipleSmallerTotal) {
  AppBannerSettingsHelper::SetEngagementWeights(0.5, 1);
  AppBannerSettingsHelper::SetTotalEngagementToTrigger(1);
  RunBannerTest("/banners/manifest_test_page.html",
                ui::PAGE_TRANSITION_GENERATED, 1, true);
}

IN_PROC_BROWSER_TEST_F(AppBannerDataFetcherBrowserTest,
                       WebAppBannerCreatedIndirect) {
  RunBannerTest("/banners/manifest_test_page.html", ui::PAGE_TRANSITION_LINK,
                1, true);
}

IN_PROC_BROWSER_TEST_F(AppBannerDataFetcherBrowserTest,
                       WebAppBannerCreatedIndirectLargerTotal) {
  AppBannerSettingsHelper::SetTotalEngagementToTrigger(5);
  RunBannerTest("/banners/manifest_test_page.html", ui::PAGE_TRANSITION_LINK,
                4, true);
}

IN_PROC_BROWSER_TEST_F(AppBannerDataFetcherBrowserTest,
                       WebAppBannerCreatedIndirectSmallerTotal) {
  AppBannerSettingsHelper::SetTotalEngagementToTrigger(1);
  RunBannerTest("/banners/manifest_test_page.html", ui::PAGE_TRANSITION_LINK,
                0, true);
}

IN_PROC_BROWSER_TEST_F(AppBannerDataFetcherBrowserTest,
                       WebAppBannerCreatedIndirectSingle) {
  AppBannerSettingsHelper::SetEngagementWeights(1, 3);
  RunBannerTest("/banners/manifest_test_page.html", ui::PAGE_TRANSITION_RELOAD,
                0, true);
}

IN_PROC_BROWSER_TEST_F(AppBannerDataFetcherBrowserTest,
                       WebAppBannerCreatedIndirectMultiple) {
  AppBannerSettingsHelper::SetEngagementWeights(1, 0.5);
  RunBannerTest("/banners/manifest_test_page.html", ui::PAGE_TRANSITION_LINK,
                3, true);
}

IN_PROC_BROWSER_TEST_F(AppBannerDataFetcherBrowserTest,
                       WebAppBannerCreatedIndirectMultipleLargerTotal) {
  AppBannerSettingsHelper::SetEngagementWeights(1, 0.5);
  AppBannerSettingsHelper::SetTotalEngagementToTrigger(4);
  RunBannerTest("/banners/manifest_test_page.html", ui::PAGE_TRANSITION_LINK,
                7, true);
}

IN_PROC_BROWSER_TEST_F(AppBannerDataFetcherBrowserTest,
                       WebAppBannerCreatedVarious) {
  AppBannerSettingsHelper::SetEngagementWeights(0.5, 0.25);

  std::string valid_page("/banners/manifest_test_page.html");
  GURL test_url = embedded_test_server()->GetURL(valid_page);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Add a direct nav on day 1.
  ui_test_utils::NavigateToURL(browser(), test_url);
  RunFetcher(web_contents->GetURL(), std::string(), ui::PAGE_TRANSITION_TYPED,
             false);

  // Add an indirect nav on day 1 which is ignored.
  ui_test_utils::NavigateToURL(browser(), test_url);
  RunFetcher(web_contents->GetURL(), std::string(), ui::PAGE_TRANSITION_LINK,
             false);
  AppBannerDataFetcher::SetTimeDeltaForTesting(1);

  // Add an indirect nav on day 2.
  ui_test_utils::NavigateToURL(browser(), test_url);
  RunFetcher(web_contents->GetURL(), std::string(),
             ui::PAGE_TRANSITION_MANUAL_SUBFRAME, false);

  // Add a direct nav on day 2 which overrides.
  ui_test_utils::NavigateToURL(browser(), test_url);
  RunFetcher(web_contents->GetURL(), std::string(),
             ui::PAGE_TRANSITION_GENERATED, false);
  AppBannerDataFetcher::SetTimeDeltaForTesting(2);

  // Add a direct nav on day 3.
  ui_test_utils::NavigateToURL(browser(), test_url);
  RunFetcher(web_contents->GetURL(), std::string(),
             ui::PAGE_TRANSITION_GENERATED, false);
  AppBannerDataFetcher::SetTimeDeltaForTesting(3);

  // Add an indirect nav on day 4.
  ui_test_utils::NavigateToURL(browser(), test_url);
  RunFetcher(web_contents->GetURL(), std::string(),
             ui::PAGE_TRANSITION_FORM_SUBMIT, false);

  // Add a direct nav on day 4 which should trigger the banner.
  ui_test_utils::NavigateToURL(browser(), test_url);
  RunFetcher(web_contents->GetURL(), std::string(),
             ui::PAGE_TRANSITION_TYPED, true);
}

IN_PROC_BROWSER_TEST_F(AppBannerDataFetcherBrowserTest,
                       WebAppBannerNoTypeInManifest) {
  RunBannerTest("/banners/manifest_no_type_test_page.html",
                ui::PAGE_TRANSITION_TYPED, 1, true);
}

IN_PROC_BROWSER_TEST_F(AppBannerDataFetcherBrowserTest,
                       WebAppBannerNoTypeInManifestCapsExtension) {
  RunBannerTest("/banners/manifest_no_type_caps_test_page.html",
                ui::PAGE_TRANSITION_TYPED, 1, true);
}

IN_PROC_BROWSER_TEST_F(AppBannerDataFetcherBrowserTest, PlayAppManifest) {
  std::string valid_page("/banners/play_app_test_page.html");
  GURL test_url = embedded_test_server()->GetURL(valid_page);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Native banners do not require the SW, so we can just load the URL.
  ui_test_utils::NavigateToURL(browser(), test_url);
  std::string play_platform("play");
  RunFetcher(web_contents->GetURL(), play_platform, ui::PAGE_TRANSITION_TYPED,
             false);

  // The logic to get the details for a play app banner are only on android
  // builds, so this test does not check that the banner is shown.
}

IN_PROC_BROWSER_TEST_F(AppBannerDataFetcherBrowserTest, NoManifest) {
  RunBannerTest("/banners/no_manifest_test_page.html",
                ui::PAGE_TRANSITION_TYPED, 1, false);
}

IN_PROC_BROWSER_TEST_F(AppBannerDataFetcherBrowserTest, MissingManifest) {
  RunBannerTest("/banners/manifest_bad_link.html",
                ui::PAGE_TRANSITION_TYPED, 1, false);
}

IN_PROC_BROWSER_TEST_F(AppBannerDataFetcherBrowserTest, CancelBannerDirect) {
  RunBannerTest("/banners/cancel_test_page.html", ui::PAGE_TRANSITION_TYPED, 1,
                false);
}

IN_PROC_BROWSER_TEST_F(AppBannerDataFetcherBrowserTest, CancelBannerIndirect) {
  AppBannerSettingsHelper::SetEngagementWeights(1, 0.5);
  RunBannerTest("/banners/cancel_test_page.html", ui::PAGE_TRANSITION_TYPED, 3,
                false);
}

IN_PROC_BROWSER_TEST_F(AppBannerDataFetcherBrowserTest, PromptBanner) {
  RunBannerTest("/banners/prompt_test_page.html", ui::PAGE_TRANSITION_TYPED, 1,
                true);
}

IN_PROC_BROWSER_TEST_F(AppBannerDataFetcherBrowserTest, PromptBannerInHandler) {
  RunBannerTest("/banners/prompt_in_handler_test_page.html",
                ui::PAGE_TRANSITION_TYPED, 1, true);
}

IN_PROC_BROWSER_TEST_F(AppBannerDataFetcherBrowserTest, WebAppBannerInIFrame) {
  RunBannerTest("/banners/iframe_test_page.html", ui::PAGE_TRANSITION_TYPED, 1,
                false);
}

}  // namespace banners
