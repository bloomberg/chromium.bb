// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/test/histogram_tester.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/banners/app_banner_manager.h"
#include "chrome/browser/banners/app_banner_metrics.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/browser/engagement/site_engagement_service.h"
#include "chrome/browser/installable/installable_logging.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace banners {

// Browser tests for web app banners.
// NOTE: this test relies on service workers; failures and flakiness may be due
// to changes in SW code.
class AppBannerManagerTest : public AppBannerManager {
 public:
  explicit AppBannerManagerTest(content::WebContents* web_contents)
      : AppBannerManager(web_contents) {}

  ~AppBannerManagerTest() override {}

  bool will_show() { return will_show_.get() && *will_show_; }

  bool is_active() { return AppBannerManager::is_active(); }

  bool need_to_log_status() { return need_to_log_status_; }

  void Prepare(base::Closure quit_closure) {
    will_show_.reset(nullptr);
    quit_closure_ = quit_closure;
  }

 protected:
  // All calls to RequestAppBanner should terminate in one of Stop() (not
  // showing banner) or ShowBanner(). Override those two methods to capture test
  // status.
  void Stop() override {
    AppBannerManager::Stop();
    ASSERT_FALSE(will_show_.get());
    will_show_.reset(new bool(false));
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, quit_closure_);
  }

  void ShowBanner() override {
    // Fake the call to ReportStatus here - this is usually called in
    // platform-specific code which is not exposed here.
    ReportStatus(nullptr, SHOWING_WEB_APP_BANNER);
    RecordDidShowBanner("AppBanner.WebApp.Shown");

    ASSERT_FALSE(will_show_.get());
    will_show_.reset(new bool(true));
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, quit_closure_);
  }

 private:
  bool IsDebugMode() const override { return false; }

  base::Closure quit_closure_;
  std::unique_ptr<bool> will_show_;
};

class AppBannerManagerBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    AppBannerSettingsHelper::SetTotalEngagementToTrigger(10);
    ASSERT_TRUE(embedded_test_server()->Start());
    InProcessBrowserTest::SetUpOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Make sure app banners are disabled in the browser, otherwise they will
    // interfere with the test.
    command_line->AppendSwitch(switches::kDisableAddToShelf);
  }

 protected:
  // Returns a test server URL to a page controlled by a service worker with
  // |manifest_url| injected as the manifest tag.
  std::string GetURLOfPageWithServiceWorkerAndManifest(
      const std::string& manifest_url) {
    return "/banners/manifest_test_page.html?manifest=" +
           embedded_test_server()->GetURL(manifest_url).spec();
  }

  void RunBannerTest(Browser* browser,
                     const std::string& url,
                     const std::vector<double>& engagement_scores,
                     InstallableStatusCode expected_code_for_histogram,
                     bool expected_to_show) {
    base::HistogramTester histograms;
    GURL test_url = embedded_test_server()->GetURL(url);
    content::WebContents* web_contents =
        browser->tab_strip_model()->GetActiveWebContents();
    std::unique_ptr<AppBannerManagerTest> manager(
        new AppBannerManagerTest(web_contents));

    // Loop through the vector of engagement scores. We only expect the banner
    // pipeline to trigger on the last one; otherwise, nothing is expected to
    // happen.
    int iterations = 0;
    SiteEngagementService* service =
        SiteEngagementService::Get(browser->profile());
    for (double engagement : engagement_scores) {
      if (iterations > 0) {
        ui_test_utils::NavigateToURL(browser, test_url);

        EXPECT_EQ(false, manager->will_show());
        EXPECT_FALSE(manager->is_active());

        histograms.ExpectTotalCount(banners::kMinutesHistogram, 0);
        histograms.ExpectTotalCount(banners::kInstallableStatusCodeHistogram,
                                    0);
      }
      service->ResetScoreForURL(test_url, engagement);
      ++iterations;
    }

    // On the final loop, we expect the banner pipeline to trigger - the
    // navigation should generate the final engagement to show the banner. Spin
    // the run loop, which should be quit by either Stop() or ShowBanner().
    base::RunLoop run_loop;
    manager->Prepare(run_loop.QuitClosure());
    ui_test_utils::NavigateToURL(browser, test_url);
    run_loop.Run();

    EXPECT_EQ(expected_to_show, manager->will_show());
    EXPECT_FALSE(manager->is_active());

    // Navigate to ensure the InstallableStatusCodeHistogram is logged.
    ui_test_utils::NavigateToURL(browser, GURL("about:blank"));

    // If in incognito, ensure that nothing is recorded.
    // If showing the banner, ensure that the minutes histogram is recorded.
    if (browser->profile()->IsOffTheRecord()) {
      histograms.ExpectTotalCount(banners::kMinutesHistogram, 0);
      histograms.ExpectTotalCount(banners::kInstallableStatusCodeHistogram, 0);
    } else {
      histograms.ExpectTotalCount(banners::kMinutesHistogram,
                                  (manager->will_show() ? 1 : 0));
      histograms.ExpectUniqueSample(banners::kInstallableStatusCodeHistogram,
                                    expected_code_for_histogram, 1);
      EXPECT_FALSE(manager->need_to_log_status());
    }
  }
};

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, WebAppBannerCreated) {
  std::vector<double> engagement_scores{0, 10};
  RunBannerTest(browser(), "/banners/manifest_test_page.html",
                engagement_scores, SHOWING_WEB_APP_BANNER, true);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest,
                       WebAppBannerCreatedImmediately) {
  std::vector<double> engagement_scores{10};
  RunBannerTest(browser(), "/banners/manifest_test_page.html",
                engagement_scores, SHOWING_WEB_APP_BANNER, true);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest,
                       WebAppBannerCreatedAfterSeveralVisits) {
  std::vector<double> engagement_scores{0, 1, 2, 3, 4, 5, 10};
  RunBannerTest(browser(), "/banners/manifest_test_page.html",
                engagement_scores, SHOWING_WEB_APP_BANNER, true);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest,
                       WebAppBannerNotSeenAfterShowing) {
  std::vector<double> engagement_scores{0, 10};
  RunBannerTest(browser(), "/banners/manifest_test_page.html",
                engagement_scores, SHOWING_WEB_APP_BANNER, true);

  AppBannerManager::SetTimeDeltaForTesting(1);
  RunBannerTest(browser(), "/banners/manifest_test_page.html",
                engagement_scores, PREVIOUSLY_IGNORED, false);

  AppBannerManager::SetTimeDeltaForTesting(13);
  RunBannerTest(browser(), "/banners/manifest_test_page.html",
                engagement_scores, PREVIOUSLY_IGNORED, false);

  AppBannerManager::SetTimeDeltaForTesting(14);
  RunBannerTest(browser(), "/banners/manifest_test_page.html",
                engagement_scores, SHOWING_WEB_APP_BANNER, true);

  AppBannerSettingsHelper::SetDaysAfterDismissAndIgnoreToTrigger(90, 2);

  AppBannerManager::SetTimeDeltaForTesting(16);
  RunBannerTest(browser(), "/banners/manifest_test_page.html",
                engagement_scores, SHOWING_WEB_APP_BANNER, true);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest,
                       WebAppBannerNoTypeInManifest) {
  std::vector<double> engagement_scores{0, 10};
  RunBannerTest(browser(), GetURLOfPageWithServiceWorkerAndManifest(
                               "/banners/manifest_no_type.json"),
                engagement_scores, SHOWING_WEB_APP_BANNER, true);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest,
                       WebAppBannerNoTypeInManifestCapsExtension) {
  std::vector<double> engagement_scores{0, 10};
  RunBannerTest(browser(), GetURLOfPageWithServiceWorkerAndManifest(
                               "/banners/manifest_no_type_caps.json"),
                engagement_scores, SHOWING_WEB_APP_BANNER, true);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, NoManifest) {
  std::vector<double> engagement_scores{10};
  RunBannerTest(browser(), "/banners/no_manifest_test_page.html",
                engagement_scores, NO_MANIFEST, false);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, MissingManifest) {
  std::vector<double> engagement_scores{10};
  RunBannerTest(browser(), GetURLOfPageWithServiceWorkerAndManifest(
                               "/banners/manifest_missing.json"),
                engagement_scores, MANIFEST_EMPTY, false);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, CancelBannerDirect) {
  std::vector<double> engagement_scores{10};
  RunBannerTest(browser(), "/banners/cancel_test_page.html", engagement_scores,
                RENDERER_CANCELLED, false);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, PromptBanner) {
  std::vector<double> engagement_scores{0, 5, 10};
  RunBannerTest(browser(), "/banners/prompt_test_page.html", engagement_scores,
                SHOWING_WEB_APP_BANNER, true);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, PromptBannerInHandler) {
  std::vector<double> engagement_scores{0, 2, 5, 10};
  RunBannerTest(browser(), "/banners/prompt_in_handler_test_page.html",
                engagement_scores, SHOWING_WEB_APP_BANNER, true);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, WebAppBannerInIFrame) {
  std::vector<double> engagement_scores{10};
  RunBannerTest(browser(), "/banners/iframe_test_page.html", engagement_scores,
                NO_MANIFEST, false);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, DoesNotShowInIncognito) {
  std::vector<double> engagement_scores{10};
  Browser* incognito_browser =
      OpenURLOffTheRecord(browser()->profile(), GURL("about:blank"));
  RunBannerTest(incognito_browser, "/banners/manifest_test_page.html",
                engagement_scores, IN_INCOGNITO, false);
}

}  // namespace banners
