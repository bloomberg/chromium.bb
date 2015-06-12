// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/banners/app_banner_data_fetcher.h"

#include "base/command_line.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "chrome/browser/banners/app_banner_data_fetcher_desktop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
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
    ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
    InProcessBrowserTest::SetUpOnMainThread();
  }

  bool HandleNonWebApp(const std::string& platform,
                       const GURL& url,
                       const std::string& id) override {
    base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE, quit_closure_);
    non_web_platform_ = platform;
    return false;
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

 protected:
  void RunFetcher(const GURL& url,
                  const std::string& expected_non_web_platform,
                  bool expected_to_show) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    scoped_refptr<AppBannerDataFetcherDesktop> fetcher(
        new AppBannerDataFetcherDesktop(web_contents,
                                        weak_factory_.GetWeakPtr(), 128));

    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    scoped_ptr<TestObserver> observer(new TestObserver(fetcher.get(),
                                                       run_loop.QuitClosure()));
    fetcher->Start(url);
    run_loop.Run();

    EXPECT_EQ(expected_non_web_platform, non_web_platform_);
    EXPECT_EQ(expected_to_show, observer->will_show());
    ASSERT_FALSE(fetcher->is_active());
  }

  void LoadURLAndWaitForServiceWorker(const GURL& url) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::TestNavigationObserver observer(web_contents, 2);
    ui_test_utils::NavigateToURL(browser(), url);
    observer.Wait();
    EXPECT_EQ("sw_activated", observer.last_navigation_url().ref());
  }

 private:
  std::string non_web_platform_;
  base::Closure quit_closure_;
  base::WeakPtrFactory<AppBannerDataFetcherBrowserTest> weak_factory_;
};

IN_PROC_BROWSER_TEST_F(AppBannerDataFetcherBrowserTest, WebAppBannerCreated) {
  std::string valid_page("/banners/manifest_test_page.html");
  GURL test_url = embedded_test_server()->GetURL(valid_page);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  LoadURLAndWaitForServiceWorker(test_url);
  RunFetcher(web_contents->GetURL(), std::string(), false);

  // Advance by a day, then visit the page again to trigger the banner.
  AppBannerDataFetcher::SetTimeDeltaForTesting(1);
  LoadURLAndWaitForServiceWorker(test_url);
  RunFetcher(web_contents->GetURL(), std::string(), true);
}

IN_PROC_BROWSER_TEST_F(AppBannerDataFetcherBrowserTest, PlayAppManifest) {
  std::string valid_page("/banners/play_app_test_page.html");
  GURL test_url = embedded_test_server()->GetURL(valid_page);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  // Native banners do not require the SW, so we can just load the URL.
  ui_test_utils::NavigateToURL(browser(), test_url);
  std::string play_platform("play");
  RunFetcher(web_contents->GetURL(), play_platform, false);

  // The logic to get the details for a play app banner are only on android
  // builds, so this test does not check that the banner is shown.
}

IN_PROC_BROWSER_TEST_F(AppBannerDataFetcherBrowserTest, NoManifest) {
  std::string valid_page("/banners/no_manifest_test_page.html");
  GURL test_url = embedded_test_server()->GetURL(valid_page);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  LoadURLAndWaitForServiceWorker(test_url);
  RunFetcher(web_contents->GetURL(), std::string(), false);

  // Advance by a day, then visit the page again.  Still shouldn't see a banner.
  AppBannerDataFetcher::SetTimeDeltaForTesting(1);
  LoadURLAndWaitForServiceWorker(test_url);
  RunFetcher(web_contents->GetURL(), std::string(), false);
}

IN_PROC_BROWSER_TEST_F(AppBannerDataFetcherBrowserTest, CancelBanner) {
  std::string valid_page("/banners/cancel_test_page.html");
  GURL test_url = embedded_test_server()->GetURL(valid_page);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  LoadURLAndWaitForServiceWorker(test_url);
  RunFetcher(web_contents->GetURL(), std::string(), false);

  // Advance by a day, then visit the page again.  Still shouldn't see a banner.
  AppBannerDataFetcher::SetTimeDeltaForTesting(1);
  LoadURLAndWaitForServiceWorker(test_url);
  RunFetcher(web_contents->GetURL(), std::string(), false);
}

IN_PROC_BROWSER_TEST_F(AppBannerDataFetcherBrowserTest, WebAppBannerInIFrame) {
  std::string valid_page("/banners/iframe_test_page.html");
  GURL test_url = embedded_test_server()->GetURL(valid_page);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  LoadURLAndWaitForServiceWorker(test_url);
  RunFetcher(web_contents->GetURL(), std::string(), false);

  // Advance by a day, then visit the page again to trigger the banner.
  AppBannerDataFetcher::SetTimeDeltaForTesting(1);
  LoadURLAndWaitForServiceWorker(test_url);
  RunFetcher(web_contents->GetURL(), std::string(), false);
}

}  // namespace banners
