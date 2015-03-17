// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/banners/app_banner_data_fetcher.h"

#include "base/command_line.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/task_runner.h"
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
    base::MessageLoop::current()->PostTask(FROM_HERE, quit_closure_);
    ASSERT_FALSE(will_show_.get());
    will_show_.reset(new bool(will_show));
  }

  void OnFetcherDestroyed(AppBannerDataFetcher* fetcher) override {
    base::MessageLoop::current()->PostTask(FROM_HERE, quit_closure_);
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
  AppBannerDataFetcherBrowserTest() : manifest_was_invalid_(false),
                                      weak_factory_(this) {
  }

  void SetUpOnMainThread() override {
    ASSERT_TRUE(embedded_test_server()->InitializeAndWaitUntilReady());
    InProcessBrowserTest::SetUpOnMainThread();
  }

  bool OnInvalidManifest(AppBannerDataFetcher* fetcher) override {
    base::MessageLoop::current()->PostTask(FROM_HERE, quit_closure_);
    manifest_was_invalid_ = true;
    return false;
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kEnableExperimentalWebPlatformFeatures);
  }

 protected:
  void RunFetcher(const GURL& url,
                  bool expected_manifest_valid,
                  bool expected_to_show) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    scoped_refptr<AppBannerDataFetcher> fetcher(
        new AppBannerDataFetcher(web_contents, weak_factory_.GetWeakPtr(),
                                 128));

    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    scoped_ptr<TestObserver> observer(new TestObserver(fetcher.get(),
                                                       run_loop.QuitClosure()));
    fetcher->Start(url);
    run_loop.Run();

    EXPECT_EQ(expected_manifest_valid, !manifest_was_invalid_);
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
  bool manifest_was_invalid_;
  base::Closure quit_closure_;
  base::WeakPtrFactory<AppBannerDataFetcherBrowserTest> weak_factory_;
};

IN_PROC_BROWSER_TEST_F(AppBannerDataFetcherBrowserTest, WebAppBannerCreated) {
  std::string valid_page = "/banners/manifest_test_page.html";
  GURL test_url = embedded_test_server()->GetURL(valid_page);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  LoadURLAndWaitForServiceWorker(test_url);
  RunFetcher(web_contents->GetURL(), true, false);

  // Advance by a day, then visit the page again to trigger the banner.
  AppBannerDataFetcher::SetTimeDeltaForTesting(1);
  LoadURLAndWaitForServiceWorker(test_url);
  RunFetcher(web_contents->GetURL(), true, true);
}

IN_PROC_BROWSER_TEST_F(AppBannerDataFetcherBrowserTest, NoManifest) {
  std::string valid_page = "/banners/no_manifest_test_page.html";
  GURL test_url = embedded_test_server()->GetURL(valid_page);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  LoadURLAndWaitForServiceWorker(test_url);
  RunFetcher(web_contents->GetURL(), false, false);

  // Advance by a day, then visit the page again.  Still shouldn't see a banner.
  AppBannerDataFetcher::SetTimeDeltaForTesting(1);
  LoadURLAndWaitForServiceWorker(test_url);
  RunFetcher(web_contents->GetURL(), false, false);
}

IN_PROC_BROWSER_TEST_F(AppBannerDataFetcherBrowserTest, CancelBanner) {
  std::string valid_page = "/banners/cancel_test_page.html";
  GURL test_url = embedded_test_server()->GetURL(valid_page);
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();

  LoadURLAndWaitForServiceWorker(test_url);
  RunFetcher(web_contents->GetURL(), true, false);

  // Advance by a day, then visit the page again.  Still shouldn't see a banner.
  AppBannerDataFetcher::SetTimeDeltaForTesting(1);
  LoadURLAndWaitForServiceWorker(test_url);
  RunFetcher(web_contents->GetURL(), true, false);
}

}  // namespace banners
