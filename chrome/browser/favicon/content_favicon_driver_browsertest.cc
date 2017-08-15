// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/favicon/content/content_favicon_driver.h"

#include "base/location.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/single_thread_task_runner.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_task_runner_handle.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/favicon/favicon_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_commands.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/favicon/core/favicon_handler.h"
#include "components/favicon/core/favicon_service.h"
#include "components/favicon/core/features.h"
#include "content/public/browser/resource_dispatcher_host.h"
#include "content/public/browser/resource_dispatcher_host_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/base/load_flags.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/url_request/url_request.h"
#include "url/url_constants.h"

namespace {

// Tracks whether the URL passed to the constructor is requested and whether
// the request bypasses the cache.
class TestResourceDispatcherHostDelegate
    : public content::ResourceDispatcherHostDelegate {
 public:
  explicit TestResourceDispatcherHostDelegate(const GURL& url)
      : url_(url), was_requested_(false), bypassed_cache_(false) {}
  ~TestResourceDispatcherHostDelegate() override {}

  void Reset() {
    was_requested_ = false;
    bypassed_cache_ = false;
  }

  // Resturns whether |url_| was requested.
  bool was_requested() const { return was_requested_; }

  // Returns whether any of the requests bypassed the HTTP cache.
  bool bypassed_cache() const { return bypassed_cache_; }

 private:
  // content::ResourceDispatcherHostDelegate:
  void RequestBeginning(net::URLRequest* request,
                        content::ResourceContext* resource_context,
                        content::AppCacheService* appcache_service,
                        content::ResourceType resource_type,
                        std::vector<std::unique_ptr<content::ResourceThrottle>>*
                            throttles) override {
    if (request->url() == url_) {
      was_requested_ = true;
      if (request->load_flags() & net::LOAD_BYPASS_CACHE)
        bypassed_cache_ = true;
    }
  }

 private:
  GURL url_;
  bool was_requested_;
  bool bypassed_cache_;

  DISALLOW_COPY_AND_ASSIGN(TestResourceDispatcherHostDelegate);
};

// Waits for the following the finish:
// - The pending navigation.
// - FaviconHandler's pending favicon database requests.
// - FaviconHandler's pending downloads.
// - Optionally, for a specific page URL (as a mechanism to wait of Javascript
//   completion).
class PendingTaskWaiter : public content::WebContentsObserver {
 public:
  PendingTaskWaiter(content::WebContents* web_contents,
                    const GURL& required_url = GURL())
      : WebContentsObserver(web_contents),
        required_url_(required_url),
        weak_factory_(this) {}
  ~PendingTaskWaiter() override {}

  void Wait() {
    base::RunLoop run_loop;
    quit_closure_ = run_loop.QuitClosure();
    run_loop.Run();
  }

 private:
  // content::WebContentsObserver:
  void DidStopLoading() override {
    if (!required_url_.is_empty() &&
        required_url_ != web_contents()->GetLastCommittedURL()) {
      return;
    }

    // We need to poll periodically because Delegate::OnFaviconUpdated() is not
    // guaranteed to be called upon completion of the last database request /
    // download. In particular, OnFaviconUpdated() might not be called if a
    // database request confirms the data sent in the previous
    // OnFaviconUpdated() call.
    CheckStopWaitingPeriodically();
  }

  void CheckStopWaitingPeriodically() {
    EndLoopIfCanStopWaiting();
    base::ThreadTaskRunnerHandle::Get()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&PendingTaskWaiter::CheckStopWaitingPeriodically,
                       weak_factory_.GetWeakPtr()),
        base::TimeDelta::FromSeconds(1));
  }

  void EndLoopIfCanStopWaiting() {
    if (!quit_closure_.is_null() &&
        !favicon::ContentFaviconDriver::FromWebContents(web_contents())
             ->HasPendingTasksForTest()) {
      quit_closure_.Run();
    }
  }

  base::Closure quit_closure_;
  const GURL required_url_;
  base::WeakPtrFactory<PendingTaskWaiter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PendingTaskWaiter);
};

}  // namespace

class ContentFaviconDriverTest : public InProcessBrowserTest {
 public:
  ContentFaviconDriverTest() {}
  ~ContentFaviconDriverTest() override {}

  content::WebContents* web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  favicon_base::FaviconRawBitmapResult GetFaviconForPageURL(
      const GURL& url,
      favicon_base::IconType icon_type) {
    favicon::FaviconService* favicon_service =
        FaviconServiceFactory::GetForProfile(
            browser()->profile(), ServiceAccessType::EXPLICIT_ACCESS);

    std::vector<favicon_base::FaviconRawBitmapResult> results;
    base::CancelableTaskTracker tracker;
    base::RunLoop loop;
    favicon_service->GetFaviconForPageURL(
        url, icon_type, /*desired_size_in_dip=*/0,
        base::Bind(
            [](std::vector<favicon_base::FaviconRawBitmapResult>* save_results,
               base::RunLoop* loop,
               const std::vector<favicon_base::FaviconRawBitmapResult>&
                   results) {
              *save_results = results;
              loop->Quit();
            },
            &results, &loop),
        &tracker);
    loop.Run();
    for (const favicon_base::FaviconRawBitmapResult& result : results) {
      if (result.is_valid())
        return result;
    }
    return favicon_base::FaviconRawBitmapResult();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ContentFaviconDriverTest);
};

// Test that when a user reloads a page ignoring the cache that the favicon is
// is redownloaded and (not returned from either the favicon cache or the HTTP
// cache).
IN_PROC_BROWSER_TEST_F(ContentFaviconDriverTest, ReloadBypassingCache) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/favicon/page_with_favicon.html");
  GURL icon_url = embedded_test_server()->GetURL("/favicon/icon.png");

  std::unique_ptr<TestResourceDispatcherHostDelegate> delegate(
      new TestResourceDispatcherHostDelegate(icon_url));
  content::ResourceDispatcherHost::Get()->SetDelegate(delegate.get());

  // Initial visit in order to populate the cache.
  {
    PendingTaskWaiter waiter(web_contents());
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_NONE);
    waiter.Wait();
  }
  ASSERT_TRUE(delegate->was_requested());
  EXPECT_FALSE(delegate->bypassed_cache());
  delegate->Reset();

  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  // A normal visit should fetch the favicon from either the favicon database or
  // the HTTP cache.
  {
    PendingTaskWaiter waiter(web_contents());
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_NONE);
    waiter.Wait();
  }
  EXPECT_FALSE(delegate->bypassed_cache());
  delegate->Reset();

  // A reload ignoring the cache should refetch the favicon from the website.
  {
    PendingTaskWaiter waiter(web_contents());
    chrome::ExecuteCommand(browser(), IDC_RELOAD_BYPASSING_CACHE);
    waiter.Wait();
  }
  ASSERT_TRUE(delegate->was_requested());
  EXPECT_TRUE(delegate->bypassed_cache());
}

// Test that loading a page that contains icons only in the Web Manifest causes
// those icons to be used.
IN_PROC_BROWSER_TEST_F(ContentFaviconDriverTest, LoadIconFromWebManifest) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL("/favicon/page_with_manifest.html");
  GURL icon_url = embedded_test_server()->GetURL("/favicon/icon.png");

  std::unique_ptr<TestResourceDispatcherHostDelegate> delegate(
      new TestResourceDispatcherHostDelegate(icon_url));
  content::ResourceDispatcherHost::Get()->SetDelegate(delegate.get());

  PendingTaskWaiter waiter(web_contents());
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
  waiter.Wait();

#if defined(OS_ANDROID)
  EXPECT_TRUE(delegate->was_requested());
  EXPECT_NE(
      nullptr,
      GetFaviconForPageURL(url, favicon_base::WEB_MANIFEST_ICON).bitmap_data);
#else
  EXPECT_FALSE(delegate->was_requested());
#endif
}

// Test that loading a page that contains a Web Manifest without icons and a
// regular favicon in the HTML reports the icon. The regular icon is initially
// cached in the Favicon database.
IN_PROC_BROWSER_TEST_F(ContentFaviconDriverTest,
                       LoadRegularIconDespiteWebManifestWithoutIcons) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL(
      "/favicon/page_with_manifest_without_icons.html");
  GURL icon_url = embedded_test_server()->GetURL("/favicon/icon.png");

  // Initial visit with the feature disabled, to populate the cache.
  {
    base::test::ScopedFeatureList override_features;
    override_features.InitAndDisableFeature(favicon::kFaviconsFromWebManifest);

    PendingTaskWaiter waiter(web_contents());
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_NONE);
    waiter.Wait();
  }
  ASSERT_NE(nullptr,
            GetFaviconForPageURL(url, favicon_base::FAVICON).bitmap_data);

  ui_test_utils::NavigateToURL(browser(), GURL(url::kAboutBlankURL));

  // Visit the page again now that the feature is enabled (default).
  {
    PendingTaskWaiter waiter(web_contents());
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), url, WindowOpenDisposition::CURRENT_TAB,
        ui_test_utils::BROWSER_TEST_NONE);
    waiter.Wait();
  }

  EXPECT_NE(nullptr,
            GetFaviconForPageURL(url, favicon_base::FAVICON).bitmap_data);
}

// Test that a page which uses a meta refresh tag to redirect gets associated
// to the favicons listed in the landing page.
IN_PROC_BROWSER_TEST_F(ContentFaviconDriverTest,
                       AssociateIconWithInitialPageDespiteMetaRefreshTag) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL(
      "/favicon/page_with_meta_refresh_tag.html");
  GURL landing_url =
      embedded_test_server()->GetURL("/favicon/page_with_favicon.html");

  PendingTaskWaiter waiter(web_contents(), landing_url);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
  waiter.Wait();

  EXPECT_NE(nullptr,
            GetFaviconForPageURL(url, favicon_base::FAVICON).bitmap_data);
  EXPECT_NE(
      nullptr,
      GetFaviconForPageURL(landing_url, favicon_base::FAVICON).bitmap_data);
}

// Test that a page gets a server-side redirect followed by a meta refresh tag
// gets associated to the favicons listed in the landing page.
IN_PROC_BROWSER_TEST_F(
    ContentFaviconDriverTest,
    AssociateIconWithInitialPageDespite300ResponseAndMetaRefreshTag) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_with_meta_refresh_tag = embedded_test_server()->GetURL(
      "/favicon/page_with_meta_refresh_tag.html");
  GURL url = embedded_test_server()->GetURL("/server-redirect?" +
                                            url_with_meta_refresh_tag.spec());
  GURL landing_url =
      embedded_test_server()->GetURL("/favicon/page_with_favicon.html");

  PendingTaskWaiter waiter(web_contents(), landing_url);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
  waiter.Wait();

  EXPECT_NE(nullptr,
            GetFaviconForPageURL(url, favicon_base::FAVICON).bitmap_data);
  EXPECT_NE(
      nullptr,
      GetFaviconForPageURL(landing_url, favicon_base::FAVICON).bitmap_data);
}

// Test that a page gets a server-side redirect, followed by a meta refresh tag,
// followed by a server-side redirect gets associated to the favicons listed in
// the landing page.
IN_PROC_BROWSER_TEST_F(
    ContentFaviconDriverTest,
    AssociateIconWithInitialPageDespite300ResponseAndMetaRefreshTagTo300) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url_with_meta_refresh_tag = embedded_test_server()->GetURL(
      "/favicon/page_with_meta_refresh_tag_to_server_redirect.html");
  GURL url = embedded_test_server()->GetURL("/server-redirect?" +
                                            url_with_meta_refresh_tag.spec());
  GURL landing_url =
      embedded_test_server()->GetURL("/favicon/page_with_favicon.html");

  PendingTaskWaiter waiter(web_contents(), landing_url);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
  waiter.Wait();

  EXPECT_NE(nullptr,
            GetFaviconForPageURL(url, favicon_base::FAVICON).bitmap_data);
  EXPECT_NE(
      nullptr,
      GetFaviconForPageURL(landing_url, favicon_base::FAVICON).bitmap_data);
}

// Test that a page which uses JavaScript to override document.location.hash
// gets associated favicons.
IN_PROC_BROWSER_TEST_F(ContentFaviconDriverTest,
                       AssociateIconWithInitialPageDespiteHashOverride) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url =
      embedded_test_server()->GetURL("/favicon/page_with_hash_override.html");
  GURL landing_url = embedded_test_server()->GetURL(
      "/favicon/page_with_hash_override.html#foo");

  PendingTaskWaiter waiter(web_contents(), landing_url);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
  waiter.Wait();

  EXPECT_NE(nullptr,
            GetFaviconForPageURL(url, favicon_base::FAVICON).bitmap_data);
  EXPECT_NE(
      nullptr,
      GetFaviconForPageURL(landing_url, favicon_base::FAVICON).bitmap_data);
}

// Test that a page which uses JavaScript document.location.replace() to
// navigate within the page gets associated favicons.
IN_PROC_BROWSER_TEST_F(
    ContentFaviconDriverTest,
    AssociateIconWithInitialPageDespiteLocationOverrideWithinPage) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL(
      "/favicon/page_with_location_override_within_page.html");
  GURL landing_url = embedded_test_server()->GetURL(
      "/favicon/page_with_location_override_within_page.html#foo");

  PendingTaskWaiter waiter(web_contents(), landing_url);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
  waiter.Wait();

  EXPECT_NE(nullptr,
            GetFaviconForPageURL(url, favicon_base::FAVICON).bitmap_data);
  EXPECT_NE(
      nullptr,
      GetFaviconForPageURL(landing_url, favicon_base::FAVICON).bitmap_data);
}

// Test that a page which uses JavaScript document.location.replace() to
// navigate to a different landing page gets associated favicons listed in the
// landing page.
IN_PROC_BROWSER_TEST_F(
    ContentFaviconDriverTest,
    AssociateIconWithInitialPageDespiteLocationOverrideToOtherPage) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url = embedded_test_server()->GetURL(
      "/favicon/page_with_location_override_to_other_page.html");
  GURL landing_url =
      embedded_test_server()->GetURL("/favicon/page_with_favicon.html");

  PendingTaskWaiter waiter(web_contents(), landing_url);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
  waiter.Wait();

  EXPECT_NE(nullptr,
            GetFaviconForPageURL(url, favicon_base::FAVICON).bitmap_data);
  EXPECT_NE(
      nullptr,
      GetFaviconForPageURL(landing_url, favicon_base::FAVICON).bitmap_data);
}

// Test that a page which uses JavaScript's history.replaceState() to update
// the URL in the omnibox (and history) gets associated favicons.
IN_PROC_BROWSER_TEST_F(ContentFaviconDriverTest,
                       AssociateIconWithInitialPageIconDespiteReplaceState) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url =
      embedded_test_server()->GetURL("/favicon/replacestate_with_favicon.html");
  GURL replacestate_url = embedded_test_server()->GetURL(
      "/favicon/replacestate_with_favicon_replaced.html");

  PendingTaskWaiter waiter(web_contents(), replacestate_url);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
  waiter.Wait();

  EXPECT_NE(nullptr,
            GetFaviconForPageURL(url, favicon_base::FAVICON).bitmap_data);
  EXPECT_NE(nullptr,
            GetFaviconForPageURL(replacestate_url, favicon_base::FAVICON)
                .bitmap_data);
}

// Test that a page which uses JavaScript's history.pushState() to update
// the URL in the omnibox (and history) gets associated favicons.
IN_PROC_BROWSER_TEST_F(ContentFaviconDriverTest,
                       AssociateIconWithInitialPageIconDespitePushState) {
  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url =
      embedded_test_server()->GetURL("/favicon/pushstate_with_favicon.html");
  GURL pushstate_url = embedded_test_server()->GetURL(
      "/favicon/pushstate_with_favicon_pushed.html");

  PendingTaskWaiter waiter(web_contents(), pushstate_url);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
  waiter.Wait();

  EXPECT_NE(nullptr,
            GetFaviconForPageURL(url, favicon_base::FAVICON).bitmap_data);
  EXPECT_NE(
      nullptr,
      GetFaviconForPageURL(pushstate_url, favicon_base::FAVICON).bitmap_data);
}

#if defined(OS_ANDROID)
IN_PROC_BROWSER_TEST_F(ContentFaviconDriverTest,
                       LoadIconFromWebManifestDespitePushState) {
  base::test::ScopedFeatureList override_features;
  override_features.InitAndEnableFeature(favicon::kFaviconsFromWebManifest);

  ASSERT_TRUE(embedded_test_server()->Start());
  GURL url =
      embedded_test_server()->GetURL("/favicon/pushstate_with_manifest.html");
  GURL pushstate_url = embedded_test_server()->GetURL(
      "/favicon/pushstate_with_manifest.html#pushState");

  PendingTaskWaiter waiter(web_contents(), /*required_url=*/pushstate_url);
  ui_test_utils::NavigateToURLWithDisposition(
      browser(), url, WindowOpenDisposition::CURRENT_TAB,
      ui_test_utils::BROWSER_TEST_NONE);
  waiter.Wait();

  EXPECT_NE(nullptr,
            GetFaviconForPageURL(pushstate_url, favicon_base::WEB_MANIFEST_ICON)
                .bitmap_data);
}
#endif
