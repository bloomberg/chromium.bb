// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/banners/app_banner_manager.h"

#include "base/command_line.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner.h"
#include "base/test/histogram_tester.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/browser/banners/app_banner_manager.h"
#include "chrome/browser/banners/app_banner_metrics.h"
#include "chrome/browser/banners/app_banner_settings_helper.h"
#include "chrome/browser/installable/installable_manager.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_switches.h"
#include "net/test/embedded_test_server/embedded_test_server.h"

namespace banners {

// All calls to RequestAppBanner should terminate in one of Stop() (not showing
// banner) or ShowBanner(). This browser test uses this and overrides those two
// methods to capture this information.
class AppBannerManagerTest : public AppBannerManager {
 public:
  explicit AppBannerManagerTest(content::WebContents* web_contents)
      : AppBannerManager(web_contents) {}
  ~AppBannerManagerTest() override {}

  bool will_show() { return will_show_.get() && *will_show_; }

  bool is_active() { return AppBannerManager::is_active(); }

  // Set the page transition of each banner request.
  void set_page_transition_(ui::PageTransition transition) {
    last_transition_type_ = transition;
  }

  using AppBannerManager::RequestAppBanner;
  void RequestAppBanner(const GURL& validated_url,
                        bool is_debug_mode,
                        base::Closure quit_closure) {
    will_show_.reset(nullptr);
    quit_closure_ = quit_closure;
    AppBannerManager::RequestAppBanner(validated_url, is_debug_mode);
  }

 protected:
  void Stop() override {
    AppBannerManager::Stop();
    ASSERT_FALSE(will_show_.get());
    will_show_.reset(new bool(false));
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, quit_closure_);
  }

  void ShowBanner() override {
    ASSERT_FALSE(will_show_.get());
    will_show_.reset(new bool(true));
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, quit_closure_);
  }

  void DidStartNavigation(content::NavigationHandle* handle) override {
    // Do nothing to ensure we never observe the site engagement service.
  }

  void DidFinishLoad(content::RenderFrameHost* render_frame_host,
                     const GURL& validated_url) override {
    // Do nothing else to ensure the banner pipeline doesn't start.
    validated_url_ = validated_url;
  }

 private:
  base::Closure quit_closure_;
  std::unique_ptr<bool> will_show_;
};

class AppBannerManagerBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
    AppBannerSettingsHelper::SetEngagementWeights(1, 1);
    AppBannerSettingsHelper::SetTotalEngagementToTrigger(2);
    ASSERT_TRUE(embedded_test_server()->Start());
    InProcessBrowserTest::SetUpOnMainThread();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Make sure app banners are disabled in the browser, otherwise they will
    // interfere with the test.
    command_line->AppendSwitch(switches::kDisableAddToShelf);
  }

 protected:
  void RequestAppBanner(AppBannerManagerTest* manager,
                        const GURL& url,
                        base::RunLoop& run_loop,
                        ui::PageTransition transition,
                        bool expected_to_show) {
    base::HistogramTester histograms;
    manager->set_page_transition_(transition);
    manager->RequestAppBanner(url, false, run_loop.QuitClosure());
    run_loop.Run();

    EXPECT_EQ(expected_to_show, manager->will_show());
    ASSERT_FALSE(manager->is_active());

    // If showing the banner, ensure that the minutes histogram is recorded.
    histograms.ExpectTotalCount(banners::kMinutesHistogram,
                                (manager->will_show() ? 1 : 0));
  }

  void RunBannerTest(const std::string& url,
                     ui::PageTransition transition,
                     unsigned int unshown_repetitions,
                     bool expectation) {
    std::string valid_page(url);
    GURL test_url = embedded_test_server()->GetURL(valid_page);
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    std::unique_ptr<AppBannerManagerTest> manager(
        new AppBannerManagerTest(web_contents));

    for (unsigned int i = 0; i < unshown_repetitions; ++i) {
      ui_test_utils::NavigateToURL(browser(), test_url);
      base::RunLoop run_loop;
      RequestAppBanner(manager.get(), web_contents->GetLastCommittedURL(),
                       run_loop, transition, false);
      AppBannerManager::SetTimeDeltaForTesting(i + 1);
    }

    // On the final loop, check whether the banner triggered or not as expected.
    ui_test_utils::NavigateToURL(browser(), test_url);
    base::RunLoop run_loop;
    RequestAppBanner(manager.get(), web_contents->GetLastCommittedURL(),
                     run_loop, transition, expectation);
  }
};
IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, WebAppBannerCreatedDirect) {
  RunBannerTest("/banners/manifest_test_page.html", ui::PAGE_TRANSITION_TYPED,
                1, true);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest,
                       WebAppBannerCreatedDirectLargerTotal) {
  AppBannerSettingsHelper::SetTotalEngagementToTrigger(4);
  RunBannerTest("/banners/manifest_test_page.html", ui::PAGE_TRANSITION_TYPED,
                3, true);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest,
                       WebAppBannerCreatedDirectSmallerTotal) {
  AppBannerSettingsHelper::SetTotalEngagementToTrigger(1);
  RunBannerTest("/banners/manifest_test_page.html", ui::PAGE_TRANSITION_TYPED,
                0, true);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest,
                       WebAppBannerCreatedDirectSingle) {
  AppBannerSettingsHelper::SetEngagementWeights(2, 1);
  RunBannerTest("/banners/manifest_test_page.html",
                ui::PAGE_TRANSITION_GENERATED, 0, true);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest,
                       WebAppBannerCreatedDirectMultiple) {
  AppBannerSettingsHelper::SetEngagementWeights(0.5, 1);
  RunBannerTest("/banners/manifest_test_page.html",
                ui::PAGE_TRANSITION_GENERATED, 3, true);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest,
                       WebAppBannerCreatedDirectMultipleLargerTotal) {
  AppBannerSettingsHelper::SetEngagementWeights(0.5, 1);
  AppBannerSettingsHelper::SetTotalEngagementToTrigger(3);
  RunBannerTest("/banners/manifest_test_page.html",
                ui::PAGE_TRANSITION_GENERATED, 5, true);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest,
                       WebAppBannerCreatedDirectMultipleSmallerTotal) {
  AppBannerSettingsHelper::SetEngagementWeights(0.5, 1);
  AppBannerSettingsHelper::SetTotalEngagementToTrigger(1);
  RunBannerTest("/banners/manifest_test_page.html",
                ui::PAGE_TRANSITION_GENERATED, 1, true);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest,
                       WebAppBannerCreatedIndirect) {
  RunBannerTest("/banners/manifest_test_page.html", ui::PAGE_TRANSITION_LINK, 1,
                true);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest,
                       WebAppBannerCreatedIndirectLargerTotal) {
  AppBannerSettingsHelper::SetTotalEngagementToTrigger(5);
  RunBannerTest("/banners/manifest_test_page.html", ui::PAGE_TRANSITION_LINK, 4,
                true);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest,
                       WebAppBannerCreatedIndirectSmallerTotal) {
  AppBannerSettingsHelper::SetTotalEngagementToTrigger(1);
  RunBannerTest("/banners/manifest_test_page.html", ui::PAGE_TRANSITION_LINK, 0,
                true);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest,
                       WebAppBannerCreatedIndirectSingle) {
  AppBannerSettingsHelper::SetEngagementWeights(1, 3);
  RunBannerTest("/banners/manifest_test_page.html", ui::PAGE_TRANSITION_RELOAD,
                0, true);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest,
                       WebAppBannerCreatedIndirectMultiple) {
  AppBannerSettingsHelper::SetEngagementWeights(1, 0.5);
  RunBannerTest("/banners/manifest_test_page.html", ui::PAGE_TRANSITION_LINK, 3,
                true);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest,
                       WebAppBannerCreatedIndirectMultipleLargerTotal) {
  AppBannerSettingsHelper::SetEngagementWeights(1, 0.5);
  AppBannerSettingsHelper::SetTotalEngagementToTrigger(4);
  RunBannerTest("/banners/manifest_test_page.html", ui::PAGE_TRANSITION_LINK, 7,
                true);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest,
                       WebAppBannerCreatedVarious) {
  AppBannerSettingsHelper::SetEngagementWeights(0.5, 0.25);

  std::string valid_page("/banners/manifest_test_page.html");
  GURL test_url = embedded_test_server()->GetURL(valid_page);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  std::unique_ptr<AppBannerManagerTest> manager(
      new AppBannerManagerTest(web_contents));

  // Add a direct nav on day 1.
  {
    base::RunLoop run_loop;
    ui_test_utils::NavigateToURL(browser(), test_url);
    RequestAppBanner(manager.get(), web_contents->GetLastCommittedURL(),
                     run_loop, ui::PAGE_TRANSITION_TYPED, false);
  }

  // Add an indirect nav on day 1 which is ignored.
  {
    base::RunLoop run_loop;
    ui_test_utils::NavigateToURL(browser(), test_url);
    RequestAppBanner(manager.get(), web_contents->GetLastCommittedURL(),
                     run_loop, ui::PAGE_TRANSITION_LINK, false);
    AppBannerManager::SetTimeDeltaForTesting(1);
  }

  // Add an indirect nav on day 2.
  {
    base::RunLoop run_loop;
    ui_test_utils::NavigateToURL(browser(), test_url);
    RequestAppBanner(manager.get(), web_contents->GetLastCommittedURL(),
                     run_loop, ui::PAGE_TRANSITION_MANUAL_SUBFRAME, false);
  }

  // Add a direct nav on day 2 which overrides.
  {
    base::RunLoop run_loop;
    ui_test_utils::NavigateToURL(browser(), test_url);
    RequestAppBanner(manager.get(), web_contents->GetLastCommittedURL(),
                     run_loop, ui::PAGE_TRANSITION_GENERATED, false);
    AppBannerManager::SetTimeDeltaForTesting(2);
  }

  // Add a direct nav on day 3.
  {
    base::RunLoop run_loop;
    ui_test_utils::NavigateToURL(browser(), test_url);
    RequestAppBanner(manager.get(), web_contents->GetLastCommittedURL(),
                     run_loop, ui::PAGE_TRANSITION_GENERATED, false);
    AppBannerManager::SetTimeDeltaForTesting(3);
  }

  // Add an indirect nav on day 4.
  {
    base::RunLoop run_loop;
    ui_test_utils::NavigateToURL(browser(), test_url);
    RequestAppBanner(manager.get(), web_contents->GetLastCommittedURL(),
                     run_loop, ui::PAGE_TRANSITION_FORM_SUBMIT, false);
  }
  // Add a direct nav on day 4 which should trigger the banner.
  {
    base::RunLoop run_loop;
    ui_test_utils::NavigateToURL(browser(), test_url);
    RequestAppBanner(manager.get(), web_contents->GetLastCommittedURL(),
                     run_loop, ui::PAGE_TRANSITION_TYPED, true);
  }
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest,
                       WebAppBannerNoTypeInManifest) {
  RunBannerTest("/banners/manifest_no_type_test_page.html",
                ui::PAGE_TRANSITION_TYPED, 1, true);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest,
                       WebAppBannerNoTypeInManifestCapsExtension) {
  RunBannerTest("/banners/manifest_no_type_caps_test_page.html",
                ui::PAGE_TRANSITION_TYPED, 1, true);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, NoManifest) {
  RunBannerTest("/banners/no_manifest_test_page.html",
                ui::PAGE_TRANSITION_TYPED, 1, false);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, MissingManifest) {
  RunBannerTest("/banners/manifest_bad_link.html", ui::PAGE_TRANSITION_TYPED, 1,
                false);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, CancelBannerDirect) {
  RunBannerTest("/banners/cancel_test_page.html", ui::PAGE_TRANSITION_TYPED, 1,
                false);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, CancelBannerIndirect) {
  AppBannerSettingsHelper::SetEngagementWeights(1, 0.5);
  RunBannerTest("/banners/cancel_test_page.html", ui::PAGE_TRANSITION_TYPED, 3,
                false);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, PromptBanner) {
  RunBannerTest("/banners/prompt_test_page.html", ui::PAGE_TRANSITION_TYPED, 1,
                true);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, PromptBannerInHandler) {
  RunBannerTest("/banners/prompt_in_handler_test_page.html",
                ui::PAGE_TRANSITION_TYPED, 1, true);
}

IN_PROC_BROWSER_TEST_F(AppBannerManagerBrowserTest, WebAppBannerInIFrame) {
  RunBannerTest("/banners/iframe_test_page.html", ui::PAGE_TRANSITION_TYPED, 1,
                false);
}

}  // namespace banners
